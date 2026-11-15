# FOnline Mechanics Sandbox

This Engine-owned fixture holds the runnable source of the mechanics recipes: one shared bootstrap, one script module per recipe, and in-server tests that run through the copyable script test harness. AngelScript is the runtime-verified backend; the Managed C# files beside it repeat the same declarations as a shape that Engine CI parses but does not compile. The recipe pages under `Docs/en/how-to/mechanics/` quote these files verbatim, and `recipes.json` is the machine-readable index that ties each page to its files, Engine symbols and smoke scenarios.

## Recipe status

| Recipe | Page | AngelScript | Managed C# | Smoke scenarios |
|---|---|---|---|---|
| Custom persistent character state | `Docs/en/how-to/mechanics/persistent-state.md` | runtime-verified | shape | `persistent-state`, `persistent-state-restart-write`, `persistent-state-restart-verify` |
| Validated action request | `Docs/en/how-to/mechanics/validated-action.md` | runtime-verified | shape | `validated-action` |
| Timed effect that survives a restart | `Docs/en/how-to/mechanics/timed-effect.md` | runtime-verified | shape | `timed-effect`, `timed-effect-restart-write`, `timed-effect-restart-verify` |
| Item behavior selected by data | `Docs/en/how-to/mechanics/data-driven-item.md` | runtime-verified | shape | `data-driven-item`, `data-driven-item-restart-write`, `data-driven-item-restart-verify` |
| Equip into a slot with one shared rule | `Docs/en/how-to/mechanics/equip-slot.md` | runtime-verified | shape | `equip-slot`, `equip-slot-client` |
| Derived stats with getters and setters | `Docs/en/how-to/mechanics/derived-stats.md` | runtime-verified | shape | `derived-stats`, `derived-stats-client`, `derived-stats-restart-write`, `derived-stats-restart-verify` |
| Put and take between a critter and a container | `Docs/en/how-to/mechanics/container-transfer.md` | runtime-verified | shape | `container-transfer`, `container-transfer-client`, `container-transfer-restart-write`, `container-transfer-restart-verify` |
| A trigger that sends a critter to another map | `Docs/en/how-to/mechanics/map-transition.md` | runtime-verified | shape | `map-transition`, `map-transition-client`, `map-transition-restart-write`, `map-transition-restart-verify` |
| NPC patrol with awareness | `Docs/en/how-to/mechanics/npc-patrol.md` | runtime-verified | shape | `npc-patrol`, `npc-patrol-restart-write`, `npc-patrol-restart-verify` |
| Interruptible long action | `Docs/en/how-to/mechanics/long-action.md` | runtime-verified | shape | `long-action`, `long-action-client`, `long-action-restart-write`, `long-action-restart-verify` |
| Lock several entities for one action (cross-cutting) | `Docs/en/how-to/mechanics/cover-transactions.md` | runtime-verified | shape | `cover-transactions` |

`runtime-verified` means the scenario passed on a built, baked server (and headless client where listed). `shape` means the C# file declares the same `///@` metadata and mirrors the control flow, but no managed build has compiled or run it.

## Layout

```text
Examples/MechanicsSandbox/
├── CMakeLists.txt, CMakePresets.json   build, bake and the RunMechanicsChecks target
├── FOnlineMechanicsSandbox.fomain      generated settings, harness values, sub-configs, resource packs
├── generate_config.py                  writes the .fomain from Engine defaults plus sandbox overrides
├── recipes.json                        recipe -> page, files, Engine symbols, smoke scenarios
├── mechanics-smoke.json                gameplay_test_runner manifest, one or more scenarios per recipe
├── run_mechanics_smoke.py              clears the restart databases and runs the manifest
├── validate.py                         configure, build, bake and run on this host
├── Content/                            Sandbox.fopro plus one prototype file per recipe
├── Maps/                               Sandbox_Map.fomap (shared) and the three maps of map-transition
├── Scripts/Sandbox.fos                 shared world creation and headless-client login
├── Scripts/Testing.fos                 byte-identical copy of the GameplayTestHarness core
├── Scripts/Recipes/<Name>.fos          one module per recipe; recipes never call each other
├── Scripts/Tests/Test_<Name>.fos       harness suites for that recipe
└── Managed/Recipes/<Name>.cs           Managed C# shape with the same declarations
```

A module's namespace equals its file stem. The AngelScript backend binds remote-call handlers by that rule, and `BuildTools/check_angelscript_hygiene.py` enforces it for every example, so the recipe namespaces are `PersistentState`, `ValidatedAction`, `TimedEffect`, `CoverTransaction`, `DataDrivenItem`, `EquipSlot`, `DerivedStats`, `ContainerTransfer`, `MapTransition`, `NpcPatrol` and `LongAction`.

## Run the checks

The example expects Engine at `Engine/` beside `CMakeLists.txt`. Inside the Engine repository, create that link once; the `.gitignore` excludes it. Remove it later with `cmd /c rmdir Engine` on Windows or `rm Engine` on Linux, never with a recursive delete.

Windows (PowerShell), from the Engine repository root:

```powershell
cd Examples\MechanicsSandbox
New-Item -ItemType Junction -Path Engine -Target ..\..
python validate.py
```

Linux (bash), from the Engine repository root:

```bash
cd Examples/MechanicsSandbox
ln -s ../.. Engine
python3 validate.py
```

`validate.py` configures the host preset into `Build/<preset>/` and builds `RunMechanicsChecks`. That target builds the headless server and headless client, checks that the generated config is current, runs a forced bake, and then runs `run_mechanics_smoke.py` in the build directory. The config sets `Baking.PrebakeOnStartup = False`, so the scenario processes start on that one bake instead of each baking again. The wrapper also creates a throwaway secure-channel key pair in the ignored `Workspace/` directory beside the config: every server needs `ServerNetwork.ChannelSecretKey`, even with networking disabled, and the config reads both halves with `$TARGET_FILE{...}`. That key is a local test identity, never a production one. Pass `--binary-dir <directory>` to use another build tree. The run passes when the runner prints `status=passed` for every scenario and exits with code 0; the JSON report is `mechanics-smoke-report.json` in the build directory.

Run one suite against an existing bake while developing; run the wrapper once first so the channel key exists. Windows (PowerShell), from the build directory:

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeTests -Testing.Filter "timed_effect."
```

Linux (bash), from the build directory:

```bash
./Binaries/Server-Linux-x64/FOMECH_ServerHeadless -ApplyConfig <sandbox>/FOnlineMechanicsSandbox.fomain -ApplySubConfig RecipeTests -Testing.Filter "timed_effect."
```

## How the scenarios prove each recipe

- **In-process tests** use the `RecipeTests` sub-config: harness on, networking off, `Server.DbStorage = Memory`. The memory database keeps records for the life of the process, so `Game.UnloadCritter` followed by `Game.LoadCritter` replays a restore without restarting.
- **Restart tests** use the `RecipeRestart` sub-config and run as two scenarios on one JSON database directory. Phase 1 checks that the directory is empty, creates and saves the state, and quits; phase 2 starts a new server process on the same directory and checks what came back. The manifest passes `-Server.DbStorage "JSON {restart_db}/<recipe>"`, and `run_mechanics_smoke.py` deletes `{restart_db}` before the run, so phase 1 always starts empty.
- **The validated action** uses a real headless client. AngelScript rejects a direct call to a `[[ServerRemoteCall]]` handler, and a stand-in would skip the parts the recipe is about: the payload limit, the engine-supplied `Player`, and the dispatcher cover of that player and its critter. The server test creates the targets and sends the client one drill: a device on another map, a device owned by someone else, a device out of reach, the valid device, and the valid device again with no energy left. The client sends the five `ActivateDevice` calls and then `DeviceDrillSent`; the handlers of one connection run in arrival order and none of them suspends, so the last call sees the final state. The manifest requires each refusal reason in the server log, and the test checks that only the valid request changed anything.
- **Client drills of the later recipes.** `equip-slot-client`, `container-transfer-client` and `long-action-client` follow the same pattern: the server test places the targets, the client sends honest and forged requests in one burst and then a final `...DrillSent` call, and the manifest requires each refusal reason in the server log. `derived-stats-client` has the client report what its own getters computed for its chosen critter and for an observed NPC. `map-transition-client` walks the player critter onto a pad and waits until the client reports the new map from `Game.OnMapLoaded`.
- **Negative controls in the test modules.** Besides the naive timed effect below, the tests keep a trigger that transfers inside the walking step (`Test_MapTransition.fos`), a completion that relies on `StopTimeEvent` alone (`Test_LongAction.fos`), a virtual property whose getter exists only on the server (`Test_DerivedStats.fos`) and a slot move that skips the request path (`Test_EquipSlot.fos`). Each one shows the failure the recipe avoids.
- **The timed effect** carries a naive variant in `Scripts/Tests/Test_TimedEffect.fos` as a negative control: its timer starts only in a property setter. The tests show that an unload and load, and a full restart, bring back both deadlines but only the recipe's timer, and that the naive boost then stays active after its deadline.
- **World restore versus `Game.LoadCritter`.** A map critter restored at server start receives `Game.OnCritterInit(cr, false)` but not `Game.OnCritterLoad`; only `Game.LoadCritter` fires both. The persistent-state tests count both events on each path, so the re-arm in the timed effect uses `OnCritterInit`.
- **Server snapshots are not covered here.** A snapshot restore brings its saved time events back after the init events have run (`ServerEngine::RestoreSnapshotTimeEvents` in `Source/Server/Server.cpp`), so the `CountTimeEvent` guard at init cannot see them and the critter can end up with two timers. The expiry handler therefore re-reads the deadline and stops a timer that finds the boost already over. The sandbox does not run a snapshot restore; this path comes from reading the source.

## Add a recipe

1. Add `Scripts/Recipes/<Name>.fos` with `namespace <Name>`, the recipe's `///@` declarations and its server and client parts. Use only Engine APIs and `Sandbox::` helpers; do not call another recipe.
2. Add `Content/<Name>.fopro` for its prototypes, with `engl` and `russ` texts.
3. Add `Scripts/Tests/Test_<Name>.fos`. Register tests from `Game.OnTestingInit` under a suite named after the recipe, for example `my_recipe.does_the_thing`; restart phases use the suites `<suite>_restart_write` and `<suite>_restart_verify`.
4. Add scenarios to `mechanics-smoke.json` with `-Testing.Filter "<suite>."` and one `[Testing] PASS <suite>.<name>` required marker per test.
5. Add `Managed/Recipes/<Name>.cs` with the same `///@` lines.
6. Add the entry to `recipes.json`: files, symbol IDs from `Docs/generated/api.json`, project metadata, backend built-ins and smoke scenarios.
7. Run `python -m pytest BuildTools/tests/test_mechanics_sandbox.py -q -p no:cacheprovider` from the Engine root, then the full check above.

## Notes

- `FOnlineMechanicsSandbox.fomain` is generated. An unpackaged run applies the `.fomain` to zeroed settings, so the file lists every Engine setting; edit `OVERRIDES` or `PROJECT_SECTIONS` in `generate_config.py`, run `python generate_config.py`, and let `--check` (run before every bake) catch a stale file. Enum settings are written as numbers because the runtime reads them as integers.
- A map critter that is persistent only because its location is persistent loses that persistence, and its database record, when it moves to another map. `Game.UnloadCritter` makes that persistence explicit first, so the unload and load tests rely on the location alone.
- Test sources are baked together with the recipes because this sandbox ships nothing. A game project keeps them out of its shipping packs; see [Add and Run Tests in a Game Project](../../Docs/en/how-to/project/project-tests.md).
- `Scripts/Testing.fos` must stay byte-identical to `Examples/GameplayTestHarness/ScriptTests/AngelScript/Testing.fos`; `BuildTools/tests/test_mechanics_sandbox.py` checks it.
- Engine CI runs only static checks on `Examples/`. The full route above needs a native build and is run on demand.
