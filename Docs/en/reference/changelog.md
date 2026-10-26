---
layout: default
title: Engine Changelog
locale: en
document_id: engine-changelog
permalink: /Docs/en/reference/changelog.html
---

# Engine Changelog

Developer-visible FOnline changes and migration notes are maintained here in English and Russian. The current development version is owned by [VERSION](https://github.com/cvet/fonline/blob/master/VERSION). [Versioning and release rules](../how-to/release/versioning.md) define CalVer, release dates, immutable tags, and compatibility boundaries.

## Unreleased

## Current - 2026-10-26

### Fixed

- Fix full-range synctime getters; repair test bytecode/lifetime/service assertions.

### Migration

- Base `3cc3a490b8f5700a6370a4192f526bcaa1d04ab1` (`2026.1.71-dev`). Rebuild/rebake; no source/data conversion. Units, APIs, formats, runtime `0.0.81` and Managed ABI `9` unchanged. Roll back matching artifacts.

## 2026.1.71-dev - 2026-10-26

### Fixed

- Fix libc++ noexcept checks, local types, search focus/selection and backup links; refresh example defaults.

### Migration

- Base `4dc5c55f59240f45e0e2d9f9f2564261b3ffa471` (`2026.1.70-dev`). APIs/settings/values/formats, runtime `0.0.81` and Managed ABI `9` unchanged; no conversion. Rebuild/regenerate docs; rollback matching artifacts.

## 2026.1.70-dev - 2026-10-26

### Fixed

- Correct square camera shifts, typed `ScriptFunc` handle storage/cleanup and project Git build-hash dependencies, including packed refs/worktrees. Snapshot fixtures use runtime metadata; retain upstream isolation and shutdown checks.

### Migration

- Base `fa15b0b7c57535af600b4212e39e2c20fa2abe4d` (`2026.1.69-dev`) → `2026.1.70-dev`: reconfigure, rebuild native/extensions and rebake. No source/save conversion: borrowed results, cover, coordinates and compiled Baker identity retain meaning. Runtime `0.0.81`, Managed ABI `9`, persisted formats and RPC wire arguments are unchanged.

## 2026.1.69-dev - 2026-10-26

### Migration

Base `6cff188c9b644732ab4e330c47a6cbdf739fcbcc` (`2026.1.68-dev`) to `2026.1.69-dev`: combine the published FOFRM/resource/time branch with master. Runtime compatibility becomes `0.0.81`; Managed ABI remains `9`. Regenerate bindings/metadata, rebuild extensions/all roles and force-rebake. Deploy/roll back matching binaries, assemblies, sources and packs together; mixed generations fail. Saved values/network units keep meaning. Validation: Windows only.

- Image import frames replace Width/Height with `isize32 Size` and NextX/NextY with `ipos32 Motion`. Placement remains `Offset`: BAM signed centers and SPR bbox/center anchors, including shared frames, are placement; FRM/ART deltas remain motion. FOFRM Move/FrameMoves become Motion/FrameMotions; obsolete keys fail before dependency resolution. Checked narrowing and SpriteResource v3 layout remain.
- Native sprite renames: GetCurSpr → GetCurrentSprite, GetSpr → GetSprite, GetDir → GetDirection, GetDirCount → GetDirectionCount, GetStateAnim/GetActionAnim → GetStateAnimation/GetActionAnimation, GetSprOffset(frame) → GetSpriteOffset(frame), RefreshParams → RefreshParameters, Sprite::SetDir and overrides → SetDirection. Update callers; GetCurrentDirection and entity/model direction APIs keep their names. Motion uses GetSpriteMotion().
- `SpriteSheet.GetWholeTicks` → `GetDuration`; image DurationMs/GetDurationMs → Duration/GetDuration (`timespan`). Model/particle intervals, MovingContext intervals/look-ahead, FogLayer.TransitionDuration and UDP retry options are typed. Ozz/backend/GPU seconds and bake-report JSON milliseconds remain boundary conversions. Delay accepts timespan; float conversions retain fractions and AS constructors check overflow.
- Replace integer FogLayer.TransitionDuration and numeric MovingContext.EvaluateProjectedHex arguments: C# `new timespan(150, 2)` / `new timespan(100, 2)`, AS `timespan(150, 2)` / `timespan(100, 2)`. MovingContext.GetElapsedTime/GetWholeTime return timespan; use it directly or `nanoseconds / 1_000_000.0` (native `to_ms<float64_t>()`) for fractional milliseconds. `milliseconds` truncates fractions. Old numeric callers fail type checking; no saved-data conversion is needed.
- Preserve shared cropped anchors in logical canvas coordinates across reference chains. Explicit zero-duration native/script sequences retain manual frame selection; file timelines stay positive. Sub-ms moving-item updates and double-precision model-cycle wrapping preserve motion and frozen endpoint poses. Interval constants retain their values/units; dependency collectors precede baker entrypoints.
- Remove TextureAnimation and native texture playback; multi-frame model textures fail validation. Static Texture/Parent, FOFRM Size and full-canvas UVs remain. Force-rebake LFMODINF schema 5; LFMODMSH/LFOZZRIG stay unchanged.
- Full bake retains unused art; Resources.usage.json selects typed Resource roots, required outputs and runtime dependencies. Packaging rejects absent/mismatched manifests. Mark resource properties Resource; force-include script/GUI/config/opaque paths through FixedType resource arrays, not constructors. Bake before packaging.

### FOFRM and resource migration

- Use PascalCase keys and sections, `Key = Value`, and one-space continuations (no final slash). Ordering/spacing otherwise remain accepted. Move direction 0 into root, resolving field conflicts; use `[HexDir 1]`..`[HexDir 5]` or `[SquareDir 1]`..`[SquareDir 7]` for the active geometry. Map legacy degree order hex 30/90/150/210/270/330 or square 45/90/135/180/225/270/315/0 to indexes 0..N-1. Old Dir/named zero fail before source resolution; no fallback.
- Rename fps → Fps; Alias → Source, remove Effect. Replace offs_x/offs_y and OffsetX/OffsetY with `Offset = X Y`, preserving carry. Replace frm/Frm/frm_N/Frm_N with exclusive nonempty Frame/Frames; remove Count, retaining active references/repeats/order. Frame is one full path; quote spaces in Frames. Legacy refusals use FO_TEMPORARY_COMPAT(FofrmLegacyDescriptor, "2026-12-31"); Alias/Effect refusals also expire 2026-12-31.
- Replace NextX_N/NextY_N with FrameOffsets: one signed-int32 X Y pair per active reference before flattening, missing pairs zero. Empty, malformed, out-of-range, wrong-count or reference-free lists fail. Add Offset and child frame offsets once without accumulation or changing imported deltas; discard child sequence placement, retain Offset carry. Int32 placement overflow fails. Size/FrameSizes resamples W H in 1..65535 while offsets stay literal pixels.
- Source inherits full collections. Frame/Frames defaults to 10 fps. Duration/FrameDurations are positive ms; local timing overrides root, direction switches retain phase across unequal timelines. Named-direction Fps/Fps0 fail; legacy clocks normalize zero to 10 fps and positive sub-ms to 1 ms. Existing Color/Encoded/Bilinear bytes stay unchanged; [Composition/filter grammar](../how-to/content/image-format.md#fofrm-grammar).
- FOFRM roots suppress transitive inputs across filtered packs/overlays. Replace direct game/model/UI/script references to consumed images/intermediate FOFRM with root wrappers; mount inputs explicitly. Output filters retain dependencies; NewName loaders declare ResolveOutput. Missing/cyclic chains or >64 FOFRM nodes fail before conversion. Fingerprints refresh dependents/SpriteInfo after same-stat edits, including on-demand bake; removing the last reference restores standalone output. Private `.baker-cache/Image/` snapshots never ship.
- Full image rebake is required for Sprite v3/SpriteInfo2. Shared pixels retain independent timing/offsets/motion. Terminal ConfigFile continuation has identical EOF/final-newline behavior; consumers validate empty values.

## 2026.1.68-dev - 2026-10-26

### Fixed

- Windows service registration writes the same `server-service-start` option that the service entry point consumes. Existing registrations using the previously unrecognized `--server-service` suffix require an administrator to update their startup command or recreate the registration with the updated utility.

- Native named command-line consumers share `CommandLineArgs` parsing, including
  runtime-host paths, config/sub-config selection, writable roots, fork and
  Windows-service flags. `GetSettings()` preserves repeated entries in order;
  `GetSettingValue(name, std::nullopt)` selects the last explicit value. Early
  reads now trim option names consistently with ordinary settings application.
  Builtin names use `SettingNames` constants generated from `Settings.inc` in
  `Common.h`; command-line-only names remain in `CommandLineSettings`. Native
  callers must replace `CommandLineSettings::CommonNonInteractive`,
  `CommonUserWritablePath` and `ClientForceEmbeddedRuntime` with their
  `SettingNames` equivalents. Setting constant identifiers use `Group_Name`,
  for example `SettingNames::Common_NonInteractive`; update native references
  from the concatenated `GroupName` spelling. Dotted config keys and their
  values are unchanged.

- Headless applications always report non-interactive mode and log errors before
  requesting failed shutdown, including when `Common.NonInteractive` is false.
  `IsNonInteractiveMode()` is available through `Application.h`; error-message
  handling belongs to the graphical and headless application implementations.

- Early reads of `Common.NonInteractive` and `Client.ForceEmbeddedRuntime` use
  the shared `CommandLineArgs::GetSettingValue` lookup, preserving the last
  occurrence, implicit flag value and each caller's value interpretation.

- `Common.NonInteractive` prevents system error dialogs from blocking unattended
  runs. Errors retain their log and traceback and take the failed `Exit` path,
  including direct messages and failures before application creation.

### Documentation

- Retain the reviewed complete AI documentation bundle limit and whole-document membership from ADR-0003; regenerate delivery after integrating the CLI/error-policy guidance.

### Migration

- Automated launchers of graphical apps should pass `--Common.NonInteractive True`
  before starting the process. Headless apps need no flag. The setting default remains `False`; embedding
  projects may enable it in their startup hook for agent-controlled sessions.
  No network, save-data or script-call conversion is required.

- Base `5c32423c1f05f1c42aaa81b02497f4fec79afca7` (`2026.1.67-dev`) -> `2026.1.68-dev`. Rebuild native applications and regenerate API/metadata and baked resources together before deployment. Existing settings keep their values and defaults; `Common.NonInteractive` is a new readonly boolean with a `False` default. No stored-data conversion is needed; wire `0.0.80`, Managed ABI `9`, resource formats and the client-host ABI are unchanged. Rollback restores the matching preceding binaries and resources.
- Native startup code uses `CommandLineArgs::GetSettingValue(name, implicit_value)` and ordered `GetSettings(name)` with shared `CommandLineSettings` names. Views borrow argv storage; callers keep the owner alive. The scalar default is `"1"`; `std::nullopt` skips valueless occurrences. Explicit empty values and negative numbers remain values. Config/sub-config requests still require a value on every occurrence, append/sub-config order and the first valid writable root are preserved. No existing script call is removed or renamed; projects adopting the helpers rebuild against the current headers. Automated graphical launchers pass the non-interactive flag before process creation; a config/startup hook takes effect only after settings load. The log/error-trace/failed-quit path replaces system error dialogs in this mode. Ordinary graphical launches keep their modal error handling; headless applications always use the non-interactive path.

## 2026.1.67-dev - 2026-10-26

### Added

- Monitor unfinished inbound server RPCs and complete queued input frames with an independent watchdog. `ServerNetwork.RemoteCallWarningTimeMs` defaults to `1000`, `RemoteCallDisconnectTimeMs` to `30000`, `RemoteCallWarningIntervalMs` to `10000`, and `MaxPendingRemoteCalls` to `256`. Zero independently disables an age reaction or admission ceiling; negative settings fail startup. Diagnostics include call identity/counts and queue readiness without arguments.

### Changed

- RPCs share the script backend's completion ownership with awaited time events. Managed registration exposes direct `Task` delegates, including `Task<T>` normalized to a declared `Task`, while preserving void wire results and synchronous Action adapters. Detached RPC completion may finish without the script pump; timer completion retains its script context. Teardown closes background completion entry before releasing backend state. Cancellation releases accounting without becoming a script fault.

### Migration

- Base `f29635c8dce0c3c4b39c30fd6d31ab0059f568db` (`2026.1.66-dev`) → `2026.1.67-dev`. Advance runtime compatibility marker `0.0.79` → `0.0.80`; rebuild native applications/extensions and rebake all script roles with matching CoreScripts. Native `BaseEngine::RemoteCallHandler` receives a fourth `shared_ptr<ScriptCallCompletion>` argument: retain it through deferred native handlers and release it on completion/abandonment. Ordinary synchronous native handlers may ignore it. Backend integrations use `FuncCallData::Completion`; only detached callbacks may opt into `CompletionOnAnyThread`. Old three-argument handler assignments fail native compilation; no compatibility shim is supplied.
- Review the opt-out settings above for legitimate long-running handlers. Disconnect closes the transport with additive `DisconnectReason::RpcRuntimeLimit = 11`; it does not cancel arbitrary Tasks, preempt code or roll back game effects. Existing reason IDs `0..10`, RPC wire arguments, persisted values, Managed generator ABI `9` and resource formats retain meaning. No saved-data conversion is required. Deploy matching binaries/resources together; mixed artifacts are refused by compatibility identity. Rollback restores the preceding matching binaries/resources and configuration together.

## 2026.1.66-dev - 2026-10-26

### Fixed

- Accept double-quoted legacy source identifiers with whitespace in `MigrationRule` for `Rename`, `Transform`, and `Remove`. Bare multiword sources, empty arguments, unclosed quotes, and replacements containing whitespace are rejected. The shared tokenizer for other metadata tags is unchanged.

### Changed

- Reject whitespace in authored prototype IDs after `$Name`/file-stem resolution and before migration resolution, hashing, or duplicate detection. Slash and dollar remain reserved for nested-section addressing.

### Migration

- Rename any authored prototype ID containing whitespace and retain the retired name only as a double-quoted source, for example `///@ MigrationRule Proto Modifier Rename "Local knowledge" LocalKnowledge`. Update native and Python migration readers together, regenerate metadata, and rebake matching resources. Existing persisted IDs remain loadable through these rules; no runtime compatibility-marker or Managed ABI change is required.

## 2026.1.65-dev - 2026-10-26

### Added

- Add native simulation pause with independent manual, named and startup ownership. Gameplay timer/movement jobs and frame-driven script processing stop while connection/player/service jobs remain responsive. Full quiescence preserves an existing pause and rejects reentrant control before taking its mutex. Expose authoritative session lifecycle state in native queries and health output.
- Capture named time events and persistent-world movement in snapshot format `4`, including issued timer ids, concrete callback signatures, remaining delays, map identity, progress and movement leases. Restore their execution on the new process clock without replaying movement-start events. Reject unsupported owners, delegate callbacks and unfinished Managed work; allow native projects to report additional counted eligibility issues.
- Preserve typed worker deadlines for submission, wake and pending reruns during a hold; validate a complete hold request before changing scheduler state. Reserve issued timer ids before startup callbacks and refuse id exhaustion instead of wrapping to zero.

### Migration

- Upgrade from `2026.1.64-dev` by rebuilding native roles and regenerating/baking matching server/client resources and Managed assemblies. The compatibility marker advances from `78` to `79`; the Managed ABI remains `9`. Keep matching binaries, assemblies and resources together on upgrade and rollback.
- Native snapshot consumers must parse and populate format `4`, including `LastTimeEventId`, named `TimeEvents` and leased `CritterMovements`. Format `1` and donor formats `2`/`3` are rejected: explicitly convert their project containers with matching world data, or recapture using matching resources. The Engine provides state/bytes rather than a container parser. Failed restore sessions must be destroyed.
- Snapshot owners must be reachable by ordinary persistent-world restore; detached/transient owners and attached movement block capture. Projects must avoid recreating captured timers in initialization callbacks and register eligibility checks for their own runtime-only state. Pause is a native embedding facility and is not enabled as a gameplay policy. Startup time is frozen through init/restore/start callbacks; durable named holds must be reapplied explicitly by the embedding controller. Existing settings, script method/event ids and persisted property values retain their meaning.

## 2026.1.64-dev - 2026-10-26

### Fixed

- Keep persistent server-entity holder ID lists unchanged in storage when critter restoration fails. Runtime pruning of the partial graph still occurs, but retrying the load retains the original durable references and error; restoring a missing child document recovers the complete graph. Successful pre-load changes and explicit migrations continue to persist normally.

### Migration

- Base `17b4dab02495de2f2877e552d7811dd48951fc41` (`2026.1.63-dev`) → `2026.1.64-dev`. No project data migration is required: public APIs, persisted schemas, resource formats, runtime compatibility `0.0.78` and Managed ABI `9` are unchanged. Existing low-level runtime pruning semantics are preserved; only failed top-level restoration stops saving the partial lists. Rebuild affected server roles and native tests, and deploy server/client resources from one matching bake. Rollback restores the preceding matching binaries and resources together.

## 2026.1.63-dev - 2026-10-25

### Fixed

- Allow CMake projects to omit `FO_APP_ICON`. An empty value now produces no icon resource instead of failing configuration with `FO_APP_ICON not specified`; explicit icon paths retain their existing generated resource.

### Migration

- Base `4588149fa4e4493ed6dddfea0a2fdf65d17c1f3e` (`2026.1.62-dev`) to `2026.1.63-dev`. This is a configuration-only expansion, not a runtime, network, persisted-data or ABI change. Runtime compatibility remains `0.0.78`, Managed ABI `9`, and the sprite container version is unchanged. No saved-data conversion is required. Rollback restores the preceding CMake sources and generated references together.
- Projects that already set `FO_APP_ICON` need no source or content migration: reconfigure and rebuild normally, preserving the same absolute icon path and `101 ICON` resource. Icon-free example and validation projects now configure successfully and receive an empty `.rc` file rather than `101 ICON ""`. Search declarations with `rg -n "FO_APP_ICON" <project-cmake-and-interface-paths>`; the previous failure was the configure-time diagnostic `FO_APP_ICON not specified`. Do not substitute another project's icon or restore a deleted asset merely to satisfy the old requirement.
- Regenerate the CMake model and reference pages with `python BuildTools/docs_cmake.py --write`, then run the focused configuration and documentation tests and the aggregate contract diff. `test_actual_scaffold_configures_without_app_icon` exercises the real validation-project configure and checks that no icon resource is emitted. Existing nonempty icon configurations require the same build/package checks as before. No runtime compatibility marker, network protocol, persisted property, resource schema or updater identity changes.

## 2026.1.62-dev - 2026-10-24

### Fixed

- Keep an executing time event registered but paused across managed await and AngelScript Yield. Apply callback-context changes and calculate the repeat deadline only after final script completion; cancellation and owner cleanup prevent late resurrection. Preserve the existing failure policy: explicit context `Repeat` survives a failed callback, `Stop` wins, and failure without explicit rearming removes the event.

### Changed

- Add `bool excludeExecuting = false` to all `Game`/entity `CountTimeEvent` overloads (callback and ID) on Managed and AngelScript. Default counts include all matching registered records; `true` excludes callbacks executing or suspended in `await`/`Yield`, through finalization. Modification and cancellation still address the registered record.

### Migration

- Native custom dispatchers use `TimeEventManager::FireAndAdvance(entity, event_id)` instead of the removed `FiredTimeEvent` result and the separate public `FireTimeEvent` / `PostFireTimeEvent` pair. The manager owns suspended execution and finalization; callers retain the owner cover and resubmit the returned optional delay. Keep dispatcher schedule/cancel hooks installed until all continuation producers have stopped. Search custom native sources for `FiredTimeEvent`, `FireTimeEvent`, and `PostFireTimeEvent`; old direct calls fail C++ compilation.

- Timer callback signatures, IDs, units, default intervals and saved-data formats are unchanged. This corrects premature completion: review code that depended on overlapping repeats or early removal of one-shot callbacks. Rebuild all affected native roles, regenerate managed assemblies and rebake matching resources together. No saved-data conversion is required. Runtime compatibility advances from `0.0.77` to `0.0.78` for the native/managed completion bridge; Managed ABI remains `9`. Mixed old/new binaries or resources are unsupported. Base: `af0770a5ef98ef31fa57c448f27ed4ce3fa86ee4` (`2026.1.61-dev`); this step is `2026.1.62-dev`.

- Audit every `CountTimeEvent` use (`rg -n "CountTimeEvent" <script-and-native-source-dirs>`). Singleton guards, active status queries and completion waits retain the default inclusive count. A one-shot remains registered until terminal completion, so snapshot coalescing must explicitly exclude execution: Managed `owner.CountTimeEvent(Callback, excludeExecuting: true)` or `Game.CountTimeEvent(id, excludeExecuting: true)`; AngelScript `owner.CountTimeEvent(Callback, true)` or `Game.CountTimeEvent(id, true)`; native `manager.CountTimeEvent(entity, func_name, id, true)`. This allows one independently queued one-shot alongside an executing snapshot. The old scheduler retained recurring records but removed one-shots when the initial call returned; neither boolean alone reproduces that mixed lifetime policy. Existing arguments, IDs and units retain their meaning. Regenerate both backends and compile Server/Client/Mapper before baking.

## 2026.1.61-dev - 2026-10-24

### Fixed

- Preserve fractional milliseconds when advancing moving item sprites, avoiding cumulative motion loss on high-refresh clients.

### Changed

- Withdraw the client/mapper `Map.BlockHex(mpos hex, bool full)` and `Map.UnblockHex(mpos hex)` proposed in PR #227. Server manual blocks are not replicated to clients, so independent local flags cannot provide consistent pathfinding and lead to repeated authoritative movement corrections. Remove the local flags, native setter and their tests instead of exposing an incomplete mechanism.

### Migration

- Base `57424cb5d2ac132b1fa8ba83e353d07cb6ca75e0` (`2026.1.60-dev`) to `2026.1.61-dev`. The published master API is unchanged; the withdrawal affects only users of earlier PR #227 heads `9c8237b0520041e3de78d878829c25c59ed5e030` and `6f31a6404207310332a39703ef784f26b054e644`. Remove client/mapper calls to `Map.BlockHex` and `Map.UnblockHex` in AngelScript/managed sources, and native uses of `MapView::SetHexManualBlock` or its `Field::ManualBlock` / `ManualBlockFull` flags. No replacement client API is provided: a future server-driven mechanism must synchronize initial state and updates before it can be exposed. The existing server methods retain their behavior and are not substitutes for client calls.
- Detect affected sources with `rg -n 'BlockHex|UnblockHex|SetHexManualBlock|ManualBlockFull' <project-source-dirs>` and review each side: client/mapper script calls fail compilation against regenerated bindings; native uses fail compilation because the declarations are absent. Regenerate metadata, rebuild affected binaries and rebake matching client/mapper resources together. Ordinary item/map obstacles and their property synchronization are unchanged. No persisted-data or resource-format conversion is required; the runtime marker remains `0.0.77`, Managed ABI `9` and sprite container v2 are unchanged. Roll back binaries and baked resources together; earlier PR builds reintroduce the unsynchronized API.

## 2026.1.60-dev - 2026-10-23

### Added

- Add the `Game.LoadCritter(crId, forPlayer, map)` overload on both scripting backends. Restore a player-controllable critter or NPC directly onto its stored map/hex/direction, preserving preload/init/load ordering, inventory, duplicate rejection and recoverable records on a failed detached load. The existing two-argument overload keeps global-map behavior; both overloads now declare their existing return-cover guarantee with `FO_PROVIDES_COVER`. Preliminary stored-field lookup uses `Game.DbGetRecord`. Local restoration emits `OnMapCritterIn` before init; initial visibility follows init so its subscriptions receive the first appearances. A callback that relocates the restored critter makes the local restore throw while preserving its valid side effects. Item-visibility fan-out stops when its observer is destroyed or leaves the map.
- Add native `ServerEngine::OnPlayerPreLogin` admission before every registration/reconnect route, and `OnPlayerPreProcess` before each complete logged-in message. Pre-login refusal or an invalidated candidate stops login before mutation; per-message refusal, destruction, disconnection or connection replacement stops dispatch; no admission policy is installed without subscribers.
- Add opt-in `ClientNetwork.RequireInterthreadTransport` (default `false`): require an in-process listener and fail immediately if it is missing, preserving the existing transport path when disabled.
- Cover host/interthread and dedicated/TCP with two clients sharing one gameplay worker: all 64 world requests must reach their handlers without lost updates.

### Migration

- Upgrade from `2026.1.59-dev` to this version by rebuilding native roles and regenerating/baking both server and client script resources with the same Engine revision. The central compatibility marker advances from `76` to `77` for the changed player-message admission boundary and API/settings metadata; clients and servers must use matching rebuilt resources. Do not mix the preceding binaries or baked metadata. Rollback restores the previous matching binaries, assemblies and resources together.
- No existing script method, event, setting, persisted property, numeric id, timer unit, resource schema or Managed ABI `9` is removed or reinterpreted. `Game.LoadCritter(crId, forPlayer)` keeps its global-map path; no persisted-data conversion is required. Native `EntityManager::LoadCritter` now takes `(cr_id, for_player, nptr<ident_t> stored_map_id, bool& is_error)`: migrate native callers to pass an output borrow for the stored map id, or `nullptr` when it is not needed, before the final error flag. The loader writes the restored map id before clearing the detached critter's `MapId`; an earlier load failure leaves the output unchanged, and destination validation remains with the caller. Native `ServerEngine::LoadCritter(cr_id, for_player, nptr<Map> map = nullptr)` now handles both destinations; ordinary two-argument calls use its default null map and explicit map calls keep stored-map restoration. Both script overloads provide cover for the fresh critter; only the map overload requires the caller's existing target-map cover. Regenerate both backends' bindings so the Managed analyzer sees the return-cover guarantee on either overload. Recompile Managed callers: `FOSYNC015` may now reject a redundant `Sync.Widen` of an already covered result. If that call only checked lifetime, replace it with `IsDestroyed`; retain acquisitions needed for other entities or after cover has been released.
- Embedded clients may set `ClientNetwork.RequireInterthreadTransport = True`; ordinary clients retain the default false. Native extensions may subscribe to the two admission events directly; these are not `///@ EngineHook` entry points or exported `Game.On*` script events. A pre-login refusal precedes registration, database work, connection replacement and login-success delivery; per-message refusal stops queued input before dispatch. Application authentication, campaign eligibility and automatic roster restoration remain project-owned. Detect optional adoption with `rg -n "RequireInterthreadTransport|OnPlayerPreLogin|OnPlayerPreProcess|LoadCritter" <project-source-paths>`; unused hooks and restore APIs need no script changes. Projects that explicitly initialize every Engine setting must add `ClientNetwork.RequireInterthreadTransport = False` to preserve normal network clients; the config baker rejects an uninitialized setting.

## 2026.1.59-dev - 2026-10-23

### Fixed

- Keep a project-frozen idle/movement selection effective for the 3D locomotion layer while a queued action advances independently. Add a regression for the overlapping selections and retain the existing queued-frozen-action control.
- Use an explicit mutable observer-slot borrow in the reentrant sprite-cache fixture, preserving the observed copy lifetime without a class reference member.

### Documentation

- Refactor the agent entry guide into concise routing and mandatory cross-cutting rules. Keep detailed native, lifecycle, managed-code, configuration and documentation contracts in their owning guides.
- Clarify semantic grouping, exception safety, property/lifetime ownership, immutable data representations and verification boundaries in the English guides and their Russian translations.

### Migration

- Base `62102cfedbd165d9ce73c600f78cb4e5750afbdc` (`2026.1.58-dev`) → `2026.1.59-dev`. No new project API/configuration or saved-data conversion: existing numeric identifiers, units and formats are unchanged. A frozen idle/movement selection now continues to disable the visual locomotion layer while an advancing queued action retains its own playback controls; logical movement is unchanged. Rebuild affected clients/Mapper. Runtime `0.0.76`, Managed ABI `9` and sprite container v2 are unchanged. Projects upgrading from earlier versions must still apply the preceding migration below. Regenerate and validate documentation; rollback restores matching preceding binaries, documentation and version together.

## 2026.1.58-dev - 2026-10-23

### Changed

- Isolate sprite-cycle accumulation, projection, phase transfer, frame selection and visual compensation in AnimationMovement. CritterHexView retains movement/anchor state; arithmetic, lifecycle, script APIs and resource formats are unchanged.

- Store ordinary sprite offsets in SpriteSheet::_sprOffset and per-frame visual motion deltas in _sprMotion. SetFrame(offset, motion) assigns matching fields; native GetSprOffset(frame) reads an ordinary offset, while GetSprMotion() replaces the former delta span GetSprOffset(). CritterHexView derives cumulative displacement, compensation and cycle phase from motion deltas. Generic GetSpriteFrameOffset/GetSpriteFrameMotion and explicit offsets,motions arrays expose the two values to project scripts.

- Store absolute frame draw offsets in SpriteSheet and share original atlas sprites instead of wrapping each offset in another AtlasSprite. Keep frame movement, light position and logical roots independent.

- Move sprite loading, factories, template/missing caches and invalidation into ResourceManager. Keep SpriteManager focused on rendering; split CritterFramesCache and Mapper-owned preview models. Preserve independent playback, retries, cleanup and script APIs.

- Remove `View.GlobalDayColorTime` and `View.GlobalDayColor` from Engine settings; projects own their day curves in code/content and submit evaluated colors with `Map.SetDayColors`.

- Move playback, movement-animation selection, preview poses, path obstacles, flat critter rendering, historical game messages and themed resources/defaults to embedding projects. Keep native animation identifiers opaque and expose explicit context/playback parameters. Android packaging preserves its platform launcher unless the project supplies an icon.

- Move all Fallout filename, anchor, frame-selection and splice rules out of ResourceManager into project scripts. Replace the two specialized events with OnCritterAnimationSprite and generic 2D sprite inspection/construction. Keep loaded-source copies and atlas-specific handle reuse isolated.

### Fixed

- Keep precise_sleep's final short interval on the current thread instead of yielding its scheduling quantum to another ready thread. Preserve the existing sleep-test deadlines; OS preemption can still delay a wait.

- Wait for the winning disconnect call's callback in the WebSocket teardown regression before checking its count. Competing calls may return while the IO thread is still reporting the disconnect; the test still requires exactly one callback before and after endpoint shutdown. Production networking is unchanged.

- Keep the nested synchronization regression bounded by inactivity and an overall deadline, retaining its critters until both worker threads join even after timeout shutdown. Slow progress no longer counts as deadlock; the production synchronization implementation is unchanged.

- Consume a sprite handle before stopping its resource, so a failing Stop cannot leave the handle active. Direct callers retain the stop exception; resolver cleanup reports it while preserving the resolver result or original exception.

- Apply Mapper property edits through native setters and post-setters after validating every affected item. Keep dirty state and undo history with the edited map when a callback switches maps, unloads the map, or deletes another affected item.

- Use the complete model/state/action tuple as the critter frame-cache key, with the shared tuple hasher; keep the original request identity across animation substitutions. Reentrant resolution returns the entry already inserted into the cache, including a cached missing result.

- Keep AngelScript value-return ABI storage intact when a native call throws; only owned handle returns need explicit release.

### Migration

- The runtime marker advances from upstream `0.0.75` to `0.0.76` for the project-owned policy/API update. Rebuild and deploy matching clients, server, metadata and resources together; mixed artifacts must fail compatibility checks. Retain upstream 36–57 migration notes below, including dynamic-item placement validation and current filesystem/persistence contracts.

- Base `aafc7fe74bb8fa35e6395e04d8c6d2a66a91831a` (`2026.1.57-dev`); target `2026.1.58-dev`. Follow the exhaustive [script-owned animation migration](../how-to/migration/script-owned-sprite-animations.md) for both removed event signatures, the native resolver replacement, both script backends, ownership, arrays, offsets, duration units, detection, build/bake order and rollback. Runtime `0.0.76`; sprite container v2 and Managed ABI 9 remain unchanged. No saved-data conversion; enum meanings remain project-owned and unchanged.

## 2026.1.57-dev - 2026-10-20

### Documentation

- Normalize native comments and JSON/SQLite backup-doc checks; describe path grammar and consolidate ClassExtension notes.

### Migration

- Base `6c88391e77d519885d54f8894d0781a7d0a14ee6` (`2026.1.56-dev`) → `2026.1.57-dev`. Existing APIs, configuration, data/resource formats and values are unchanged; wire `0.0.75`, Managed ABI `9`. No conversion. Regenerate/validate docs, rebuild matching native metadata/applications and rebake resources. Rollback restores the matching preceding artifacts.

## 2026.1.56-dev - 2026-10-20

### Added

- Add `fs::write_file_durable_new` for exclusive new-file writes with descriptor flush, reusing the native path and stream primitives. Existing names are preserved; staged atomic replacement remains the existing `fs::write_file`/`fs::replace_file` contract.
- Add instance-local `DataBase::TryInsert` and typed `CompareExchange`, with pending-queue visibility and producer synchronization against ordinary mutations, snapshot restore and queue clear. Export `Game.DbTryInsertRecord` and `Game.DbCompareExchangeRecordString` for integer and string keys on both script backends.

### Removed

- Remove the unused `ClassExtensionAttribute` placeholder. It never bound a global function as a class method; C# uses language extension methods, and no corresponding AngelScript tag is planned.

### Fixed

- Reject embedded NUL at every filesystem path boundary and malformed UTF-8 before native conversion on every platform. Use shared UTF-8 scalar validation in a single pass with the NUL check, preserving its U+FFFD error-sentinel rejection and rejecting surrogate encodings; validate native-to-string output as well. Honor I/O conversion errors and preserve literal POSIX backslashes. Make result-based file operations consistently `noexcept`, handling exceptions locally at native conversion, owned stream construction/read and iterator operations. Convert unexpected C++ exceptions at internal STL boundaries to an operation failure. Keep path converters, standard-stream interop and caller streams/visitors potentially throwing; preserve partial directory listings and hash-to-EOF semantics.
- Give the standalone LibreSSL archive probe lean Windows headers so WinCrypt macros cannot collide with its X509 declarations; retain its AES, TLS, X509 and SRTP assertions with assembly enabled and disabled.
- Track generations of active database reads so a concurrent reader cannot hide a completed commit from an earlier read or let a conditional mutation accept a stale value.
- Preserve internal spaces in JSON/SQLite storage directories by parsing the complete trimmed remainder after the provider name, including connection strings with outer whitespace.

### Migration

- Base `8af9a71307196a1937c79389e83036d2face5ceb` (`2026.1.55-dev`); update `2026.1.56-dev`. Existing file helpers, ordinary DB methods and valid connection strings retain their meaning. New-file creation exposes its name during writing and flushes file content only; use a private stage plus atomic replacement when readers must see complete publication. Conditional DB success queues a mutation in one owning instance; it does not establish durable commit, a cross-process transaction or native backend CAS.
- Rebuild native consumers/Baker and rebake all script roles so generated API/metadata and assemblies agree. Callers may adopt the two new methods explicitly; no existing call must be renamed and no game feature is enabled automatically. JSON/SQLite path values need no escaping for internal spaces; literal quote stripping is not part of the connection grammar. Memory/Mongo tokens and disabled-provider diagnostics are unchanged.
- No entity/property, JSON/CBOR/BSON/oplog, snapshot, baked-resource or wire format conversion is introduced. The manual runtime marker advances from `0.0.74` to `0.0.75`; Managed ABI `9` stays unchanged; regenerate the source-derived API/compatibility metadata and deploy matching binaries/resources. Rollback uses the matching previous build/resources without data conversion. Validate native contention/pending/restore barriers, both script bindings, local-provider reopen with spaces, and filesystem success/refusal on each claimed platform.
- Filesystem callers must pass complete NUL-free UTF-8 paths on every platform. Non-UTF-8 POSIX file names are no longer supported by `fs`. Pure converters now reject invalid paths with `filesystem_error`; result primitives refuse them without acting on the prefix. Rebuild native consumers for the read/write/hash/comparison/listing exception specifications. Result-based operations now report standard interop errors through failure results; oversized whole-file reads are refused before buffer allocation. The Engine terminate-on-OOM policy is unchanged. Visitor and caller-stream exceptions still propagate; valid paths, empty-path conventions and file publication/durability guarantees are preserved.

- Remove `[ClassExtension]`; use standard C# extension methods and rebuild scripts with current CoreScripts. The compiler rejects the removed attribute. Unannotated helpers, entry-point attributes, bindings, data/resource formats, wire and ABI `9` are unchanged. No conversion or extra compatibility bump. Rollback restores matching previous CoreScripts/assemblies.

## 2026.1.55-dev - 2026-10-20

### Changed

- Reject map-local unsynchronized Common or Client configuration overrides on dynamic items in Mapper and MapBaker, including constant project properties without `Persistent`. Check both server and client property candidates, including hidden items. Preserve server-only authoring and placement context; configure permanent geometry on prototypes. Automatic mesh coalescing applies only to static items.
- Recache every registered client item footprint cell when `NoBlock`, `ShootThru`, or `LightThru` changes.
- Keep the standalone LibreSSL regression probe portable on Windows by selecting lean Windows headers before the OpenSSL headers; preserve its real OtherName, IP/mask and SRTP checks.

### Migration

- Base `d947827d42ab7a8a6c5f0c168cdea5680423ddf9` (`2026.1.54-dev`); update `2026.1.55-dev`. Network compatibility remains `0.0.74`; no new property flags or wire layout are introduced.
- Move differing unsynchronized Common or Client values from dynamic map placements into prototypes or inherited variants, or use an existing synchronization channel when runtime changes are required. Rebuild affected native targets and rebake resources. Explicit zero, false, and empty values count as overrides. Static placements, server-only fields, names, types, defaults and persistence flags retain their meanings. Existing saved worlds require the embedding project's normal content-update review; a map-source migration is not a database migration. Rollback restores matching baseline binaries and authored resources.

## 2026.1.54-dev - 2026-10-19

### Fixed

- Serialize WebSocket close-state mutations with the backend I/O loop. Disconnect and wrapper destruction enqueue weak-only close work, preventing direct engine-thread `close()` from racing websocketpp termination. Native coverage exercises peer abort, repeated disconnect, once-only notification and a wrapper retained after listener destruction.
- Restore complete AI documentation delivery after the maintained corpus outgrew its size budget. The manifest owns the budget; route documentation and repository tests no longer duplicate its current value. Document membership, deterministic UTF-8/LF output and the failure on overflow remain unchanged.
- Remove the current target's dedicated ZIP when packaging fails, including disk exhaustion during ZIP writing or a later installer failure. Cleanup reports removal errors without masking the original packaging exception; `SingleZip`-only aggregate handling is unchanged.
- Preserve const-qualified string-search results in the vendored LibreSSL X509 and SRTP parsers. The IP-constraint parser keeps a separate mutable cursor for its owned copy. This removes host-header compiler warnings without casts, warning suppression or crypto/API changes; native probes cover OtherName, IP/mask, SRTP, AES and TLS with assembly enabled and disabled.

### Migration

- Base `d74075954cbf3742f0f717ff2b23ff309aa735a4` (`2026.1.53-dev`); update `2026.1.54-dev`. Regenerate affected API models/references with `python BuildTools/docs_api.py --write`, `python BuildTools/docs_reference.py --write` and `python BuildTools/docs_public_api.py --write`, then run `python BuildTools/docs_prepare.py`, validate with `python BuildTools/docs_validate.py`, then build the site with `python BuildTools/docs_site_build.py`. Read the delivery budget from `ai_delivery.full_context.max_bytes` in `Docs/documentation-manifest.json`. `BuildTools/tests/test_docs_ai_delivery.py` retains independent overflow and byte-determinism checks; do not truncate documents or disable the size guard. Generated delivery files remain ignored build outputs and require no follow-up commit.
- The central network compatibility marker advances from `0.0.73` to `0.0.74` for the WebSocket lifecycle fix. Regenerate metadata, rebuild matching native client/server applications, Baker and tests, and rebake scripts/resources; deploy the matching set together. Mixed old/new artifacts must be rejected by the compatibility check. Native/script APIs, configuration, saved data, resource schema and Managed ABI `9` retain their meanings; no saved-data conversion is required. The Python packaging fix takes effect on its next invocation; retry after resolving the underlying storage or installer error. Rollback restores the entire matching previous artifact set, delivery policy and failed-ZIP retention without data conversion.


## 2026.1.53-dev - 2026-10-19

### Fixed

- Preserve persistent custom-entity trees during graceful server shutdown. Runtime teardown releases the singleton's and registered holders' inner references before dropping parent links and registry ownership; it does not run ordinary custom-entity deletion or change durable holder IDs.

### Documentation

- Clarify native property components for Critter, Location and other entity owners: bool markers, qualified fields, inherited disabling, managed Has accessors and persisted-name migrations.

### Migration

- Baseline `2860bb2755621976d199883e254d66808a659ec1` (`2026.1.52-dev`) to this `2026.1.53-dev` working change. The affected path is `ServerEngine::Shutdown` → `EntityManager::DestroyAllEntities`, shared by scripting backends and database providers. Ordinary gameplay `DestroyCustomEntity` still deletes the explicitly destroyed entity and its children.
- No callable, setting, property, identifier, storage format, network compatibility marker (`0.0.73`) or Managed ABI (`9`) changes; no project source or data conversion is required. Existing persistent holder IDs and child documents must survive stopping the process. Reconfigure and rebuild affected native server/test roles, regenerate metadata and rebake the matching resource set after the version/revision change.
- Previously deleted documents cannot be reconstructed from surviving secondary indexes. Restore a verified backup through the owning project's recovery procedure when affected data has already been lost; do not erase indexes to hide missing entities. Rollback uses matching binaries/resources and a verified backup; the preceding build retains the destructive shutdown defect.
- Acceptance owner: `ServerShutdownPreservesPersistentCustomEntityTreesAcrossRestarts` in `Source/Tests/Test_ServerEngine.cpp`. It must preserve exact IDs, holder shape and marker values through two graceful restarts using real JSON storage, including a custom child of a persistent NPC. Embedding projects must also verify their used backend and account-entry path on an isolated database.

## 2026.1.52-dev - 2026-10-18

### Added

- Expose server-only readonly `Critter.MapTransfersLocked` in both scripting backends. It reads the existing native nesting counter under critter cover, allowing a caller to defer a transfer before invoking its native guard. It creates no stored property or new lock; the native guard remains authoritative.

### Migration

- Base `219fbd7568f2388f4a3b47e5a22fb38b8e259a0d` (`2026.1.51-dev`); update `2026.1.52-dev`. Rebuild native consumers and rebake scripts/resources together to regenerate metadata and Managed method bindings. Existing script calls and input meanings remain valid; persisted data, resource formats, network migration version `0.0.73` and Managed ABI `9` are unchanged. No saved-data conversion is required. The new export changes generated client/server compatibility and Managed method-manifest hashes even though their schema versions are unchanged. Rebuild and deploy matching server/client binaries and rebaked scripts; do not mix baseline and updated artifacts. Rollback restores the matching baseline artifacts without data conversion.
- Read the property under live critter cover immediately before the transfer, with no yield or cover release between the check and call. A true value is transient: release cover before waiting and revalidate the operation after reacquiring it. The getter does not reserve a future transfer. Validate nested lock reads through `EntityManagerCppApi/MapTransfersLockedExportObservesNestedLocks` and the embedding project's transfer continuation path.
- Increase the reviewed full-context documentation capacity by 16 KiB to 2,342,912 bytes so the complete accumulated migration notes remain available. The generator still fails on overflow; no document is truncated or excluded by this capacity update. Runtime and saved data are unaffected.


## 2026.1.51-dev - 2026-10-18

### Fixed

- Retain persistent NPC and inventory database records during critter unload and failed restoration cleanup. Explicit NPC destruction still deletes its records; player-character ownership no longer decides whether unregistering deletes a database document.
- Cover persistent NPC unload/load, carried item ownership, permanent destruction and recovery after a missing inventory record with native regressions.

### Migration

- Base `1f2f8e4e55b51899ba07ad83f9e05287d365a78f` (`2026.1.50-dev`); update `2026.1.51-dev`. The internal native declaration in `Source/Server/EntityManager.h` changes from `UnregisterCritter(ptr<Critter> cr)` to `UnregisterCritter(ptr<Critter> cr, bool delete_from_db)`. Find custom native callers with `rg -n 'UnregisterCritter\(' Source <native-extension-directory>`: replace `UnregisterCritter(cr)` with `UnregisterCritter(cr, false)` for unload/failed-load cleanup, or `UnregisterCritter(cr, true)` only for permanent NPC destruction. The old one-argument call fails compilation; no default argument or compatibility overload is provided. Keep the NPC-only destruction precondition and inventory teardown ordering. Script APIs and unaffected native callers retain their meaning.
- Reconfigure and rebuild native server and tests; refresh revision metadata through the normal matching Baker/resource workflow. Run `ServerUnloadLoadKeepsPersistentNpcAndItemRecordsUntilDestroy` and `ServerFailedNpcLoadKeepsRecoverableRecords`, checking committed critter/item documents before unload, after unload, after reload and after final destruction. NPCs must already be explicitly persistent; following a player does not promote a temporary entity. No property, configuration, authored-content, database-format, resource-schema, Managed ABI `9` or runtime compatibility `0.0.73` change is introduced. Existing IDs, item ownership and temporary-entity semantics are preserved; no saved-data conversion is required, and previously deleted documents cannot be reconstructed by this fix.
- Deploy matching rebuilt artifacts. Rollback restores the preceding binaries/resources without data conversion and restores the old NPC-record deletion defect. Earlier runtime acceptance does not qualify a newer Engine base; this local history transfer does not claim a new build, gameplay qualification or publication.

## 2026.1.50-dev - 2026-10-18

### Fixed

- Reject ACM streams declaring zero samples before decoding or writing baked output. Both effect and music paths retain path-qualified failures, and the focused loader/baker regressions cover whole-pack and selected-file baking without creating an output for the rejected source.

### Migration

- Base `370b268aa01099e731a2d93a8ae65222eb06c816` (`2026.1.49-dev`); update `2026.1.50-dev`. Rebuild the ACM-enabled Baker and native tests and run `AudioBaker` and `AudioBakerAcm`; repair any source rejected with `ACM stream declares no samples` and rebake affected ACM input. Valid non-empty streams retain their sample, rate, channel-path and truncated-body padding meanings. No script, setting, prototype, persisted-data or resource-schema migration is required. Network compatibility `0.0.73`, Managed ABI `9` and updater boundaries are unchanged; no new mixed-version restriction or deployment order is introduced. Rollback restores the matching previous binaries/resources without data conversion; it also restores acceptance of the invalid empty stream. These instructions do not claim a completed native/runtime qualification.

## 2026.1.49-dev - 2026-10-17

### Fixed

- Route Windows console Ctrl+C and Ctrl+Break through normal application quit after frontend initialization. The handler only latches a process-wide atomic flag; console-close/logoff/shutdown events and forced termination retain their existing behavior.
- Enforce the existing read-only contract for startup `Common.UserWritablePath`: reject nonempty config assignments before substitution or append parsing, and keep empty generated assignments from clearing the resolved root.

### Migration

- Base `c7a19d2c833c3b16c741fb02cf170c14e5c251c5` (`2026.1.48-dev`); update `2026.1.49-dev`. Search authored inputs with `rg -n 'Common\.UserWritablePath\s*=' --glob '*.fomain*' --glob '*.cfg'`. Remove every nonempty assignment from main/sub-configs, local-config cache entries and packaged internal config sources; supply `--Common.UserWritablePath <path>` at process startup instead. Nonempty assignments now fail with `SettingsException: Writable root is resolved before settings` at runtime or baking. Inspect local-cache content through its owning configuration tool; an opaque cache file is not a text config. Empty generated assignments remain valid no-ops. CLI priority, `*`, installer-marker selection and portable empty-root behavior are unchanged.
- Rebuild native applications and Baker, then run the normal bake to refresh revision metadata and validate packaged configs. Verify CLI resolution, empty config values, main/sub/local rejection, and normal shutdown after both console events using a dedicated child process/console. Windows service control and POSIX signals are unchanged; a GUI application without a console still uses its ordinary quit action. No script API, persisted-data, map/resource-schema, network compatibility marker, runtime `0.0.73` or Managed ABI `9` change is required. Roll back matching binaries/resources without save conversion.
- Windows console Ctrl+C and Ctrl+Break handling is installed unconditionally after frontend initialization. The handler only latches a quit flag; each application chooses safe exit points by servicing `IsQuitRequested()`. Custom native loops that need cooperative early termination should poll `IsQuitRequested()`. Batch tools and tests that do not observe quit can finish their current work (no immediate cancellation guarantee). POSIX signal setup and Windows service control are unchanged. The subprocess matrix covers both Windows events for application-loop quit and deferred observation.

## 2026.1.48-dev - 2026-10-17

### Fixed

- Prepare a 3D model sprite's first alpha hit mask during sprite updates, before the first hover or click. Picking only reads the completed mask and requests refresh; render updates start later copies on demand, at most once per 100 ms, and poll pending pixels every frame. This prevents a background item from being selected while the first mask is cold and keeps hovered silhouettes current without per-frame copies.

### Migration

- Base `617a0d6f1af16a6bcf3da951e5299ddf31c26158` (`2026.1.47-dev`); update `2026.1.48-dev`. Rebuild native client/Mapper consumers and tests; run the embedding project's normal bake to refresh revision metadata. No script, configuration, prototype, baked-format or persisted-data migration is required: pixel-precise alpha picking and all existing input meanings are preserved. Network compatibility `0.0.73` and Managed ABI `9` are unchanged; no new mixed-version restriction or deployment ordering is introduced. Rollback restores baseline native binaries without data conversion.
- Validate `ModelSpritePreparesItsHitMaskDuringRenderUpdate`, `ModelSpriteHitMaskThrottlesRequestedCopiesButPollsPendingPixels`, the retry/redraw tests, and first clicks with and without prior hover in a visible client. Bound the embedding game's cursor-entity cache lifetime so a cached background item cannot hide a newly completed mask. Verify the interaction window separately from command submission. Picking adds no rectangle fallback or GPU wait; asynchronous readiness polling remains independent of the 100 ms copy interval.

## 2026.1.47-dev - 2026-10-17

### Fixed

- Move the `ConfigFile` `ParseLargeConfig` sampling benchmark into the opt-in `ConfigFilePerformance` case (`[!benchmark][config]`), matching the hidden-benchmark layout of `TextureAtlasLayoutPerformance` and `PropertiesPerformance`. Ordinary functional selection no longer repeats the measurement on each of the 29 `SECTION` paths; the explicit `ConfigFilePerformance` case retains the unchanged `ParseLargeConfig` measurement on the `128x12` input. No CI-wide speedup is measured or claimed.
- Acknowledge a pending `work_thread` clear request on the worker before queue inspection, dispatch or wait. Queued jobs are discarded and `clear()` returns for every queue shape, including empty and future-dated queues and across pause/resume; a request landing around an earlier acknowledgement notification no longer strands `clear()` on the worker's done signal.

### Migration

- Base `d498fcf07d1425cf31e71b060fe8aeae87aeb37d` (`2026.1.46-dev`); next update `2026.1.47-dev`. Rebuild the test executable; the existing functional `ConfigFile` filter still runs the same 136 assertions, and performance consumers select `ConfigFilePerformance` (or the explicit benchmark tag) instead of relying on `ConfigFile`'s incidental sampling. Parser, API, settings, scripts, content, save, network, updater and resource-schema contracts, Managed ABI `9` and runtime `0.0.73` remain unchanged; no game or data migration and no rebake is required solely for this test scheduling fix. Rollback restores the previous test layout without data conversion. No publication is implied by this draft.
- Rebuild Essentials and the native consumers and tests that link it, including the focused `WorkThread` suite with its new clear-acknowledgement liveness regression. API, settings, scripts, content, save, network, updater and resource-schema contracts, Managed ABI `9` and runtime `0.0.73` remain unchanged; no data conversion and no resource-format migration. Embedding projects keep their ordinary revision-metadata update and bake qualification for this Engine revision; nothing here changes authored content, so that qualification is the normal validation step rather than a format migration. Rollback restores the matching prior artifacts without data conversion and restores the clear-acknowledgement defect. No publication or release qualification is implied by this draft.

## 2026.1.46-dev - 2026-10-17

### Fixed

- Generate nested Managed and AngelScript setting access from shared canonical path parsing. Preserve full runtime keys; built-ins stay read-only, Managed properties stay get-only, and AngelScript custom settings retain their setters.
- Keep AngelScript group paths with underscores distinct and reject property/group collisions before script use. Reject empty segments in the shared parser; Managed also checks escaped C# name collisions.
- Normalize environment `FO_DOTNET_DIR` paths only on Windows, preserving POSIX path characters and the derived runtime, payload and stamp locations.
- Build full-file stages from the native filesystem parent, preserving literal leading spaces, POSIX backslashes and platform path interpretation. Resource-path trimming and separator normalization no longer affect disk filenames.
- Release cleanup ownership of a stage name after successful publication, so a later writer reclaiming that name cannot be unlinked by the completed writer.
- Cover literal-path replacement, readonly cache/bootstrap publication refusal, CRT readers without delete sharing and loaded-library safety. Qualify Windows in-use publication: a successful replacement is atomic, but an open stream or image can still make publication fail without changing the destination.

### Migration

- `a627da59d2b408e4b618b125bcd7766879c03d41` (`2026.1.45-dev`) → `2026.1.46-dev`: `Domain.Name` stays `Settings.Domain.Name`; `Android.GradleDependency.Sample` now generates nested Managed access; AngelScript keeps nested access with unambiguous group types. `SettingsPath.A_B.Value` and `SettingsPath.A.B.Value` are distinct. Empty segments raise `Setting path` (wrapped as `Managed setting` by the Managed baker); correct the authored key. Managed namespace/property collisions after C# escaping also raise `Managed setting`. Existing valid grouped paths keep their meaning; AngelScript still accepts valid ungrouped custom settings. Identical canonical Managed keys still deduplicate.
- AngelScript internal `GlobalSettingsGroup_*` types now frame segment lengths. Replace explicit generated types with direct access: `GlobalSettingsGroup_Android_GradleDependency group = Settings.Android.GradleDependency; string value = group.Sample;` becomes `string value = Settings.Android.GradleDependency.Sample;`. Old explicit type names fail compilation; there are no aliases. Search authored scripts for `GlobalSettingsGroup_` before rebuilding.
- Reconfigure; rebuild native components and Baker; regenerate/compile Server/Client/Mapper, including all AngelScript bytecode; run full then incremental baking. Deploy matching sources/assemblies/bytecode/resources; rollback restores the prior matching set. No configuration renaming or save conversion. Script leaf signatures, IDs, configuration value meaning, save/wire formats, Managed ABI `9` and runtime `0.0.73` stay unchanged. See [configuration and data sources](settings/configuration-and-data-sources.md).
- Native `fs::write_file` string/byte-span signatures and `fs::replace_file` remain current; no script-backend, role or project caller rename is required. Existing path spelling keeps its native meaning. A valid relative `" dir/data.bin"` now writes into that exact directory; POSIX `"/tmp/back\\slash/data.bin"` retains its literal backslash instead of treating it as a separator.
- Reconfigure/rebuild native roles and Baker, then regenerate and rebake all affected sides with the matching Engine revision. Check `DiskFileSystem`, `DiskFilePrimitives`, `CacheStorage`, `ClientRuntimeBootstrap*` and `NullRenderer`, plus the enabled scripting backend and embedding bake/runtime routes. Windows-only tests do not qualify POSIX execution.
- `false` still means no new file was published: retain the previous value/selector, close blocking readers when appropriate, or use `fs::disk_read_file` for delete-sharing reads. Native loaded DLL images and CRT streams are not promised replaceable on every Windows version/filesystem. Do not implement delete-then-rename or a fallible rollback around this helper.
- No setting, property, prototype, save, network, updater or resource-schema conversion; runtime `0.0.73`, Managed ABI `9`, serialized values and successful-bake ordering remain unchanged. Atomic visibility is not power-loss directory durability. Rollback restores matching 45-dev binaries/resources without save conversion and restores its literal-path rejection defect. See [Essentials](native/essentials.md#filesystem-compression-sockets-and-work-threads) and [baking](../explanation/content-pipeline/baking.md).

## 2026.1.45-dev - 2026-10-16

### Fixed

- Apply the declared C++ formatting profile to the Managed C# property-callback registrar generator. Emitted callback names and C# bytes remain unchanged.
- Accept scissor origins outside the render target in the Null renderer, matching GPU backend clipping for partially offscreen GUI panels. Negative scissor extents and out-of-bounds texture access remain invalid.

- Remove terminal periods from Windows file-replacement comments to follow the authored-comment policy; native behavior is unchanged.

### Migration

- Base `5cc571ef888291d132de3fdab30ac0914a10ef48` (`2026.1.44-dev`); next update `2026.1.45-dev`. No API, setting, script, authored content, persisted data, runtime `0.0.73`, or Managed ABI `9` conversion is required.
- Reconfigure/rebuild tools and headless applications to carry the new Engine version and Null scissor fix; check formatting, the existing callback-name regression, and `NullRenderer`. Rollback uses the matching 44-dev artifacts without save conversion and restores its stricter Null scissor-origin validation. No publication or platform qualification is implied by this draft.

## 2026.1.44-dev - 2026-10-16

### Fixed

- Make `fs::write_file` the sole atomic whole-file helper for strings and byte spans, including embedded NUL and empty input. Exclusive sibling staging, flush and atomic replacement preserve the previous destination on failure and retain open readers/mapped DLLs. `disk_write_mode::create_new` rejects existing paths; no `write_file_atomic` alias remains.
- Apply the shared helper to cache entries, managed staging, client session/bootstrap markers, Baker outputs/reports/fingerprints, particle dependency snapshots, JSON records and SQLite restore scratch files. Failed cache publication retains the existing entry; `RemoveEntry` is explicit invalidation. Windows rename names are NUL-terminated, and the standalone path probe remains independent of Engine linkage.
- Reconcile cumulative contract dispositions without removing earlier review records. Retain complete owning guides and migration history in the reviewed 2 MiB + 224 KiB AI bundle; align its manifest, EN/RU guides and policy tests. Required start routes, membership and oversize rejection remain unchanged.

### Migration

- Baseline: `1d88ee229d2d5208dd6c4c86814e032598a06e4c` (`2026.1.43-dev`). This single unpublished update consolidates the local atomic-write fixes and documentation reconciliation; apply the published migrations through 43 before adoption. The intermediate local versions are not release steps.
- Native `fs::write_file(path, string_view)` and byte-span signatures are retained; writes now replace complete files instead of truncating the destination. Audit parent-directory permissions, symlinks, hard links and destination metadata. Identity-dependent streaming/in-place writes use `fs::disk_write_file`; check write and flush results. The WinApi/Posix `open_new_write_file` wrappers add optional `exclusive=false`, preserving two-argument callers. Experimental local `write_file_atomic` calls must use `write_file`; no second whole-file policy or compatibility alias exists.
- Reconfigure/rebuild Essentials, Common, Client, Server, Tools and Managed consumers; regenerate bindings, references, EN/RU, site/routes and AI outputs, then rebake using the rebuilt Baker. Check filesystem/cache/database/selector/baking tests, publication with open assembly readers and old destination survival on failure. `SetDataChecked == false` means the new value was not committed; `SetString` logs failures. Preserve the previous complete package for rollback.
- No managed caller, setting, prototype, persisted property, save, network, updater or resource-schema conversion is introduced relative to published 43. JSON keys/content, selector validation, successful marker ordering and unchanged timestamps retain meaning. Runtime compatibility stays `0.0.73`, managed ABI identity `9`, and sprite container v2. Atomic visibility does not promise directory durability across power loss. See [Essentials](native/essentials.md#filesystem-compression-sockets-and-work-threads).

## 2026.1.43-dev - 2026-10-15

### Fixed

- Send authoritative `CritterDir` on direction rejection in active/finished stop reconciliation; positions unchanged.

### Migration

- `54f4e6393c60aad893dbd60b596a9b7c8088ba0c` (`2026.1.42-dev`) → `2026.1.43-dev`; compatibility `0.0.72` → `0.0.73`. Rebuild baker/server/clients/Mapper; regenerate metadata; rebake/deploy both resource sides together. Wire/API, Managed ABI 9, positions/directions and saves retain meaning; no conversion. Rollback restores matching baseline binaries/resources.

## 2026.1.42-dev - 2026-10-15

### Fixed

- Reject static map blockers in server gag-aware path queries, including when a dynamic gag occupies the same cell. Honor per-instance static removal and preserve permitted dynamic-gag routes.
- Correct the equal-screen-row test for hexagonal and square geometry.

### Changed

- Resize sparse two-dimensional grids by visiting stored cells instead of scanning the old and new map areas; preserve overlap values and discard out-of-bounds cells.

### Migration

- Base `eedcaa955ff4be79e43384766f7a47cbda0788d1` (`2026.1.41-dev`); update `2026.1.42-dev`. Rebuild native consumers and run the normal bake to refresh revision metadata. Map sources, baked formats, script signatures, configuration, persisted data, runtime `0.0.72` and Managed ABI `9` remain unchanged; no data conversion is required.
- Static blocking retains its existing meaning: gag predicates can permit dynamic gag items, but cannot bypass an intact static blocker. Remove workarounds that depend on false-positive routes through static walls; use instance static removal when the static item is actually removed. Verify blocked exact targets, batch reachability, dynamic gags and the removal transition. Rollback restores matching base binaries/resources without save conversion.

## 2026.1.41-dev - 2026-10-15

### Fixed

- Fix links/SPARK bounds, C# style/EOF, locals, DB sort/POSIX startup; document lock transfer.

### Migration

- `c727e6f549fe54095e21dad4c272b99761083e0f` (40-dev) → 41-dev. API/config/data, ABI 9, runtime 0.0.72 unchanged. Rebuild roles/runtime cache; rebake SPARK; regenerate docs. Validate these fixes and effect order. Rollback: matching base binaries/docs/packs.

## 2026.1.40-dev - 2026-10-14

### Changed

- Append matching text packs before one stable sort; grow capacity geometrically and bound binary sizes. Sort on success only; failures preserve their error and may leave dirty entries. Successful retries restore order.
- Accept `const_span<uint8_t>` in native `TextPack::LoadFromBinaryData` and its append helper; decoded strings own their storage.
- Sort text packs explicitly after successful loading; the single completion path needs no scope guard.
- Make `scope_success` destruction conditionally `noexcept`: callback failures propagate on success; unwind skips the callback.
- Correct docs/export comments: no-index `GetText` already chooses uniformly random variants; indexed selection retains load order.
- Normalize Windows separators in environment-supplied `FO_DOTNET_DIR` before CMake evaluation; retain cache precedence. Make directory checks compare paths and give codegen fixtures the real helper definitions.

### Migration

- Base `92ee4cb67d66e586c026e181e576b830deb7112a` (`2026.1.39-dev`); update `2026.1.40-dev`. Rebuild all native consumers/baker for the guard header, refresh metadata and bake. No script/config/content/save conversion; text layout, language filtering, collection substitution and duplicate order remain.
- Invalid sizes raise `DataReadingException` before allocation; failed loads do not sort/rollback, and dirty reads reject. Check guard throw/unwind/return, batch equivalence, retry and configured languages. Runtime `0.0.72`, Managed ABI 9 and schemas remain; rollback needs no save conversion.
- Recompile native callers of `TextPack::LoadFromBinaryData`; vector arguments convert implicitly. Update explicitly typed member-function pointers to the span signature. Encoded bytes and script APIs remain unchanged.

## 2026.1.39-dev - 2026-10-14

### Fixed

- Respect placement overrides of `Static` and `Hidden` when selecting client map items. Cover both directions with a native regression checking exact item IDs.
- Retain the complete documentation bundle with a reviewed 32 KiB budget increment; preserve document membership and oversize rejection.

### Migration

- Base: `48c437b01e85858e7c702c4be834dd647cd180cb` (`2026.1.38-dev`); next `2026.1.39-dev`. Rebuild Baker, rebake all maps and ship matched server/client pairs. Saved maps load the updated static layer. No API, configuration, binary/network schema, managed ABI or save conversion; existing placement properties retain their authored meaning. Rollback restores the previous Baker/maps without save conversion.

## 2026.1.38-dev - 2026-10-14

### Fixed

- Managed C# callbacks register canonical component names (`Weapon.UseMode`), preserving flat underscores across getter and setter overloads.

### Migration

- `4c594d1bef` (`2026.1.37-dev`) → `2026.1.38-dev`: rebuild Baker; rebake server/client/Mapper. Signatures, IDs, saves, wire layouts, ABI `9` and runtime `0.0.72` stay unchanged. Deploy matching generated sources/assemblies; rollback restores old Baker/assemblies. No source/save migration.

## 2026.1.37-dev - 2026-10-14

### Changed

- Register all test sources unconditionally and place backend guards inside their translation units. Keep native engine/entity/map/network/rendering checks available to both backends; split AngelScript binding checks into `Test_AngelScript*` suites with shared header fixtures.
- Add real ManagedScriptBaker/runtime bridge coverage for server, client and Mapper: initialization, events, call marshalling, exceptions and isolation of simultaneous servers.

### Fixed

- Register managed `Dictionary<K,V>` native-call entry points using the existing dictionary bridge instead of skipping their signatures as unsupported, and copy dictionary callback results through the caller's accessor with managed objects rooted during conversion.
- Await and capture the ASIO accept-callback test's detached exception report before restoring global test state, preventing its intentional diagnostic from contaminating later logging tests.
- Track packed-to-loose Git refs with file-only CMake dependencies.
- Filter EntityHolder sides before holder validation; retain shared-holder stubs.
- Honor Game-holder Persistent children and descendants; ordinary children stay temporary.

### Migration

- Base: `91d8bf21ac89e5342db17aab8e25332e049e900e` (`2026.1.36-dev`); next `2026.1.37-dev`. Reconfigure the test build after adopting the source split. Update path-based test tools to the `Test_AngelScript*` filenames and focused Catch2 filters to the prefixed names of split AngelScript cases. Managed runtime tests require the .NET SDK for the configured target framework and the managed runtime payload. Rebuild all enabled roles and test both backends; regenerate metadata, references and bake markers with matching binaries. No gameplay, script API, configuration, save or network conversion is required: existing dictionary signatures now use their existing bridge. Managed ABI `9`, runtime `0.0.72`, sprite v2, resource and network schemas retain their identity. Deploy matching binaries/runtime payloads together; rollback restores the preceding matching binaries without converting saved data.
- Rebuild server/baker and rebake metadata. Reconcile historical IDs of children that were never stored. Declarations, saved-document formats and protocol remain unchanged.

## 2026.1.36-dev - 2026-10-13

### Fixed

- Name `Test_AudioBaker`, `Test_AudioBakerAcm`, and `Test_AudioManager` in audio validation; ACM still requires `FO_AUDIO_ACM`.
- Align the bilingual AI delivery ADR with its dated measurement, preserving complete documents and the byte budget.
- Normalize six diagrams to LF without visual changes; retain four dated verification records as historical evidence.

### Migration

- `cd5c1168f8f4bdab63a2909ceacb049ba87e070a` (`2026.1.35-dev`) → `2026.1.36-dev`: documentation, generators and tests only. No API, script, configuration, resource, persisted-data or DB conversion. Audio semantics, ABI `9`, runtime `0.0.72`, sprite v2, updater and network schemas are unchanged.
- Run `docs_audio.py --write`, `docs_diagrams.py --write`, then `docs_prepare.py` from `BuildTools/`. Check audio documentation tests, diagrams, EN/RU hashes, version, site/routes and complete AI delivery. Use matching code/build/package metadata; no forced rebake. Native audio and audible platform checks remain separate. Rollback restores baseline documentation, generators and metadata; saved data is unchanged.

## 2026.1.35-dev - 2026-10-12

### Fixed

- Fix GCC's empty ImageBaker test return and managed nullable formatting; preserve runtime behavior.
- API checks retain source equality and explicit classification: 2573 symbols, 2572 experimental, one internal.
- Compact AI bundle URL labels; preserve complete documents, URLs, membership and the existing byte limit.

### Migration

- Base: `d5bef2f554019e4c6c45366a1f422c11e9229878` (`2026.1.34-dev`); next `2026.1.35-dev`. Keep prior ownership, ACM and Arcanum adoption. API, config, storage, sprite v2, ABI `9` and runtime `0.0.72` are unchanged.
- Rebuild and rebake all roles; run formatting/API/native tests and `python BuildTools/docs_prepare.py` for references, EN/RU, site and AI. Rollback uses matching baseline-34 code, tests and metadata; no game-state conversion.

## 2026.1.34-dev - 2026-10-12

### Fixed

- Keep the first loaded sprite sheet independent of its cached prototype. Trim both frame arrays; bound copying and initialize copy size/anchor at frame/direction zero.

### Migration

- Base `5170fa03ccdcc8efb6a592f9aba3baebfd544cb6` (`2026.1.33-dev`); update `2026.1.34-dev`. Rebuild native clients/Mapper and restart. No script/config/content or database conversion: first/last selection, directional deltas, state/action tags and atlas sharing retain meaning.
- Copies now follow the existing independent-playback contract from the first load. First/last extraction cannot shorten later loads; last-frame deltas still sum both axes. Check repeated loads, trimmed copies and visible animation/directions/stop-start.
- Runtime `0.0.72`, Managed ABI 9 and sprite container v2 stay unchanged. Reuse baked images; refresh normal generated metadata/bake markers and deploy matching binaries. Rollback restores the baseline binaries without converting saves.

## 2026.1.33-dev - 2026-10-12

### Added

- FOFRM `Alias` retains a binary sheet's directions, anchors, deltas and timing; ART `$r=0..7` selects one rotation.

### Fixed

- Correct DAT 2.1 names/case/slashes/flags and ART header, palettes, rotation-major tables, bounded raw/RLE streams and placement.
- Preserve DAT/ZIP/embedded/cached-directory/nested-image exception context; diagnose byte bounds, truncated records, decoded lengths and compression trailers.
- Exclude empty HTML anchors from search outside code; preserve visible text, IDs and literal examples within existing byte limits.

### Migration

- Base `6875e109534e94657c37fed55c559550901e9313` (`2026.1.32-dev`); update `2026.1.33-dev`. Enable `FO_DATA_SOURCE_DAT`/`FO_IMAGE_ART` only when needed (defaults OFF); disabled inputs name the option. `DataSource-Dat`/`ImageBaker-Art` own decoding; FOFRM delegates to `ImageImport::Context::Resolve`. Other formats, callbacks and options retain meaning.
- `$r3`/`$R3` remains palette 3: replace donor `Alias = Actor$r3.art` with `Alias = Actor$r=3.art`. Ordinary `frm` accepts the selector; `Alias` retains the whole sheet and rejects another FOFRM. Follow the [ART guide](../how-to/content/image-format.md#art) for selector validation, palette fallback, limits, timing and category-dependent placement; preserve its source-directory categories.
- Detect inputs with `rg --files -g '*.art' -g '*.dat' -g '*.fofrm'` and `rg -n 'Alias|\$[rR]=?[0-7]' <source-roots>`. Reconfigure/rebuild baker, runtime and metadata; force-rebake affected packs with `BakeResources`. Restart and visually check palettes, anchors, deltas, aliases and both map geometries.
- Handle cached short reads as `DataSourceException`; repair named archives and refresh directory indices. Missing lookups/log ownership remain unchanged. Sprite container v2, runtime `0.0.72`, ABI identity 9, updater/package schemas and persisted data stay unchanged from the base. Deploy resources/binaries together; rollback restores both. No save/database conversion or proprietary assets are required.


## 2026.1.32-dev - 2026-10-12

### Fixed

- Normalize native RefType lifetime/binding comments; runtime tokens and generated API stay unchanged.
- Bind FOFRM Count provenance to its loader and reading context; reject the ART loader.
- Enforce the published 2 MiB + 192 KiB governance budget, complete documents and oversize rejection.
- Match translation checks to 4,866 entries: 2,576 API, 106 audio and 79 CMake descriptions; reject missing/stale translations.
- Restore the optional-archive-provider heading and anchor.
- Match the executable CMake check to 58 options.
- Exclude managed `bin`/`obj` from documentation sites; rendered audits retain authored sources.
- Cap search summaries at 224 characters, retaining full-document postings and the byte budget.

### Migration

- Baseline: `2026.1.31-dev`, `b197060d30d01ac74d7a203039a6bdb379c17948`. Retain RefType ownership (29), required ACM adoption (30), and bounded observation (31): ABI `9`, runtime `0.0.72`. No new script, config, resource-schema or stored-data migration; runtime semantics stay unchanged.
- Run `python BuildTools/docs_prepare.py` for models, EN/RU, snippets, site/routes and AI outputs; validate governance, provenance, translations and versioning. Rebuild/package with `2026.1.32-dev`. Rollback restores baseline-31 docs/tests and matching builds without changing game state or ownership.

## 2026.1.31-dev - 2026-10-11

### Added

- `Player.ViewMapArea(map, hex, radius)` provides a live, fixed, bounded map view while retaining a controlled global-map critter. Dynamic subjects are filtered by inclusive hex distance; entering and leaving subjects are updated from movement and item events. Clients initially center the camera and mask the exterior independently of regular fog settings.
- `Player.ReturnFromViewMap()` restores the controlled critter's normal display. Existing detached `ViewMap` remains a full-map administrative view.

### Fixed

- Bounded map cameras accept normal scrolling without moving their visibility area. Resizing preserves the camera position within authored scroll boundaries. The exterior mask uses an overridable primitive fog effect with an inward smooth edge and full exterior opacity.
- Bounded map views respect authored camera scroll boundaries when centering, zooming and resizing. The visibility area remains centered on its focal hex even when the camera must stop at a map edge.

### Migration

The `ViewMap` network message now carries an `int32` radius after its focal hex (zero for the existing full-map view). Network compatibility advances from `0.0.71` to `0.0.72`; rebuild both endpoints and their script metadata together. No persisted map, item or critter data migration is required: view subscriptions and bounds are runtime-only. Bounded views send current positions at hex transitions and omit future movement paths. Returning requires the complete controlled-critter initial-info graph; global-map group information is sent by the project under its group cover.

Rebake resources to include `Effects/Primitive_ViewAreaFog.fofx` when adopting the smooth bounded-view edge. Embedding projects can override that effect and must keep the exterior opaque. Camera scrolling requires the project's normal input handlers on its bounded-view screen; the visibility center and radius remain server-owned.

Baseline: `e1a3815ff96c7d88f47314984e9d9242e37bc585` (`2026.1.30-dev`); this rebased bounded-view update is `2026.1.31-dev`. Apply the preceding Managed RefType ownership migration and rebuild/rebake all roles together.

## 2026.1.30-dev - 2026-10-11

### Added

- Add the optional built-in Interplay ACM audio importer as `FO_AUDIO_ACM`, OFF by default. `AudioBaker-Acm.*` owns the per-decoder unpacker state and Fallout channel convention; `AudioBaker` registers it only when enabled. Move the decoder regression fixtures into Engine and test automatic Vorbis baking plus disabled requests.

### Migration

- Base: `2026.1.29-dev`, `3a366761742204d15aa86726cc23168c9b7ec81d`; this update is `2026.1.30-dev`. WAV/Ogg defaults, exact paths, playback, volumes and existing source meanings stay unchanged. The new importer is Baker-only and backend/platform independent; no new third-party library is required.
- Projects using a local ACM loader must set `SetOptionValues(FO_AUDIO_ACM ON)` before `StartProjectGeneration`, remove their ACM source/test entries from `AddEngineSources`, and remove the loader include and `audio_baker->AddLoader(LoadAcmAudio, {"acm"})` registration from `SetupBakersHook`. Keep other project bakers/hooks. Direct decoder users include `AudioBaker-Acm.h` and use `AudioBakerAcm::Decoder`, `AudioBakerAcm::DecoderException` and `AudioBakerAcm::Load(string_view, FileReader)`; their declarations exist only when enabled. Search CMake, presets, environment and native extensions for `AcmDecoder`, `LoadAcmAudio`, `.acm` and `FO_AUDIO_ACM` before migration. Projects using only WAV/Ogg need no source migration and leave the option OFF.
- Preserve a case-insensitive `sound/music/` prefix for stereo ACM; all other ACM paths are mono even if the header says stereo. Keep positive source sample rates and complete non-empty PCM frames. The unpacker retains the original wrap/padding arithmetic; encoded output retains the authored path and uses the existing Vorbis payload. Add `acm` to project `Audio.SoundFileExtensions` only when the inspection catalog needs it; the Engine default remains `wav ogg`.
- Reconfigure, rebuild bakers/affected native targets, then bake all resource packs with `Audio`. `FO_AUDIO_ACM` participates in `FO_BAKING_CONFIG_HASH`, so missing/changed fingerprints force a full bake and are written only on success. Disabled collection and targeted ACM requests fail before incremental skipping with `Audio importer is disabled; enable FO_AUDIO_ACM` and the path. Confirm focused decoder tests, enabled/disabled bake tests, real ACM pack counters/paths/channel shapes and active-device client playback.
- No persisted entity/database conversion, migration rule, network marker (upstream `0.0.71`), sprite/resource schema or host/runtime ABI change. Ship the newly baked Vorbis resources at their existing paths. Rollback restores the previous baker and the project's local loader/build entries together, followed by a full bake; do not mix loader registrations from both owners.

## 2026.1.29-dev - 2026-10-11

### Fixed

- Factory-backed Managed C# constructors create usable native objects and share their factory lease. Native types without a factory have no public default constructor; dynamic DTOs keep ordinary construction.
- Refcounted wrappers automatically retain one native reference. Factories and `PassOwnership` transfer it; other results, callback/event arguments and collection elements acquire it. Forgotten leases queue engine-thread cleanup; shutdown cleans queued/live leases before entity collection/unbind. Native calls keep wrappers alive.

### Added

- Standard `IDisposable`/`using` ownership hides `__Factory`, `__AddRef` and `__Release` from the public managed API. Native zero-argument `Dispose` actions run before reference release: `FogLayer.Dispose()` removes its layer and closes the wrapper in one call.

### Migration

- Baseline: `6a1f03880a84e097cfedee6d331178aad89e390d` (`2026.1.28-dev`); apply versions 26–28 below first. The earlier audited `16f1608794` sources have the zero-pointer constructor defect. All managed roles change; native/AngelScript signatures, IDs, defaults and property/data schemas stay unchanged.
- Replace `__Factory()` with `new`, remove manual `__AddRef()`, and replace `__Release()` with `Dispose()`/`using`. Engine-returned wrappers retain their native targets automatically. C# aliases share disposal; independent wrappers have independent ownership. Dynamic DTOs and non-refcounted wrappers lack this interface.
- Find default construction of `SpritePattern`, `FogLayer`, `VideoPlayback`, `TimeEventContext` and `MovingContext` and use their creating APIs. Compile every role to catch aliases and target-typed `new()`.
- Keep `Map.DrawMapSprite` holders through the last rendered frame, then `StopDraw()` and `Dispose()`. A `using` scope must cover that frame; GC timing is unspecified. Direct `fog.Dispose()` and `using` perform both native and wrapper cleanup; remove former two-step cleanup. Repeated disposal and disposal after shutdown are harmless. Disposed pointer access and boxed/packed arguments reject use. Cleanup failures retain ownership for retry without repeating a completed native action.
- Rebuild matching native components and rebake every role with ABI generator identity `9`. Runtime compatibility advances from `0.0.70` to `0.0.71`; binaries/resources and client/server sides upgrade together. The preceding unpublished ABI `8`/runtime `0.0.70` bindings also require rebuilding. Follow [rebuild ordering](../how-to/build/generated-content.md). No persisted-data or backup conversion is needed.
- Rollback rebuilds/rebakes matching previous sources for all sides, restoring their prior manual ownership and constructor behavior. Earlier component, storage, baking-lock, document-migration and server-grid requirements below still apply.
- Generator tests verify the public surface; lifetime tests cover automatic acquisition, native cleanup, GC, retries, aliases, constructor leases, shutdown and backend isolation. Runtime acceptance covers properties, drawing, `using`/disposal and orderly shutdown. See [lifetime rules and validation](../how-to/scripting/managed-csharp.md).

## 2026.1.28-dev - 2026-10-11

### Fixed

- Normalize six display-query EOFs and make metadata `hstring` locals explicit; keep both read-only proxy const containers and analyzer reasons.
- Sync the snippet regression with the reviewed 470-example/311-normative corpus; condense config prose and normalize four CMake comments. Full normative coverage and rejection tests remain enabled.
- Reconcile image-import source ownership and native custom-loader migration guidance with the extracted modules.

### Migration

- Base: `2026.1.27-dev`, `5ef9334595f45522c8d580c25e22ed4556ab137b`; apply its locking/build-glue migration first. No runtime/API/options/ABI/resource/save change; compatibility stays `0.0.70`. For `2026.1.28-dev`, reconfigure/rebuild matching hosts and rebake role metadata/scripts; regenerate EN/RU, snippets, site/routes and AI delivery. Check formatting, native/process-lock tests and versioning.

## 2026.1.27-dev - 2026-10-11

### Added

- Add move-only `fs::exclusive_file_lock` and non-blocking `fs::try_lock_file`, with non-truncating acquisition, Unicode/long paths, explicit release, move transfer and non-inherited native descriptors. Web returns an unavailable lock.

### Fixed

- Own the canonical baking output with a sibling file lock before report/hash deletion and through final reporting. A refused run preserves the previous output, even when reusing a MasterBaker. Forced rebuilds leave the lock inode in place.
- Serialize AngelScript role and script-order pack compilation only under `AS_DEBUG`; acquire the working directory's diagnostic lock across Baker processes. Release keeps configured parallelism.
- Let the native baker alone write the compiled resource build hash inside the output lock. Remove the unlocked CMake write after the baker exits; native binary hash generation is unchanged.

### Migration

- Engine version is `2026.1.27-dev`, the next minor after published master `2026.1.26-dev`. Reconfigure and rebuild native bakers and version metadata, then run the normal bake. No script binding, network marker, sprite format, database or save conversion changes.
- Leave `<BakeOutput>.lock` beside the output directory and `AS_DEBUG.lock` in the working directory. Ignore these generated files in embedding repositories; their existence is harmless. Do not delete them to recover contention. Stop or wait for the owner, check permissions and retry. Lock failures occur before output/report/hash mutation; filesystem roots are rejected as BakeOutput.
- `fs::try_lock_file` does not create parents, truncate content or wait. A closed lock also represents unavailable paths and unsupported Web, not only another owner. On POSIX the exclusion is advisory and requires participants to use the same file inode. Keep the lock owner alive until protected work and reporting finish.
- Debug bakes with AngelScript need a writable diagnostic-lock location; separate working directories permit independent debug bakes. Ordinary Release compilation remains parallel. This guard applies to MasterBaker, not arbitrary external AngelScript processes.
- Custom CMake bake wrappers must not write Resources.build-hash after native Baker returns. The compiled baker writes it before lock release; retain WriteBuildHash only for native binary markers. The revision/hash contract stays compiled-source based. On rollback restore the old baker and build glue together, and complete a bake before consuming its outputs.

## 2026.1.26-dev - 2026-10-11

### Changed

- CMake `TemporaryCompat` dates restrictions, including inactive branches. Database selection has no Web veto.
- Projects select adjacent guarded archive/database/image modules with positive `SetOptionValues`; source/header registration is unconditional. Memory and PNG/TGA/FOFRM remain mandatory. ZAR/TIL share their guarded decoder with preserved channel order.
- JSON files use ordinary JSON, SQLite BLOBs use CBOR, BSON belongs only to Mongo. External ZIP/BOS is optional; Embedded/APK retain zlib/minizip. Declared baking-impact options invalidate resources independently of project revision; database and service options are excluded.

### Migration

- Include `Engine/BuildTools/cmake` and project CMake files in explicit `temporary_compat.py` inputs. Mark restrictions with `TemporaryCompat(Id "YYYY-MM-DD")`; CI owns expiry, keeping configuration reproducible.
- Baseline: `2026.1.25-dev`, `16f1608794eafeb6c05001b997071105dbc403b9`. [Component selection](../how-to/build/project-configuration.md#optional-native-components) lists all options, roles, dependencies and diagnostics; [persistence](../explanation/persistence/index.md#storage-formats-and-upgrade-boundary) defines the new schema/limits.
- Remove `FO_DISABLE_MONGO`/`FO_DISABLE_SQLITE` from CMake, presets, environment and cache (`cmake -UFO_DISABLE_MONGO -UFO_DISABLE_SQLITE ...`). Explicitly enable used `FO_DATABASE_JSON`, `FO_DATABASE_SQLITE`, `FO_DATABASE_MONGO`, `FO_DATA_SOURCE_DAT`, `FO_DATA_SOURCE_ZIP` and `FO_IMAGE_*` through `SetOptionValues`. All optional defaults are OFF; retired options and disabled requests fail, with no aliases/fallback.
- Search native callers for `ImageBaker::FrameShot`, `FrameSequence`, `FrameCollection`, `LoadFunc`, `MAX_DIRS_MINUS_ONE`: replace the prefix with `ImageImport::`. Loader argument four becomes `const ImageImport::Context&`; use `context.Files` and `context.Resolve(path)`. `AddLoader` argument three optionally maps output paths. Include adjacent `DataBase-{Memory,Json,SQLite,Mongo}.h`; factories become `DataBaseMemory::Create`, `DataBaseJson::Create`, `DataBaseSQLite::Create`, `DataBaseMongo::Create`, initialization becomes `DataBaseSQLite::InitializeRuntime`. BSON converters/allocator initialization are Mongo-private.
- Use new empty JSON/SQLite directories. Old Extended JSON/BSON BLOBs have no reader or migration; retain old binaries/storage together for rollback. Mongo representation remains unchanged. No entity/property conversion rule is introduced; codecs preserve current AnyData values as documented in persistence.
- Reconfigure, regenerate bindings/metadata, rebuild affected native roles together, fully rebake and package matching outputs. `Resources.baking-config-hash`/report `bakingConfigHash` are separate from `Resources.build-hash`; missing/changed baking configuration removes stale outputs. The [fingerprint contract](../how-to/build/project-configuration.md#baking-configuration-fingerprint) includes geometry, models/effects, scripting and particles as well as archive/image loaders. Compatibility advances `0.0.69` → `0.0.70`: no mixed binaries/snapshots; deploy matching client/server metadata and roll back complete binaries/resources/storage sets. Updater protocol, client-runtime ABI identifier and baked sprite version stay unchanged.
- Regenerate contracts/snippets, EN/RU, site/routes and AI delivery. Validate minimal/full database/archive/image/baker suites, provider reopen/recovery and disabled-use failures; report platform/runtime proof separately. No publication is implied.

## 2026.1.25-dev - 2026-10-10

### Fixed

- Detach critter links during destruction without publishing an attachment snapshot from either endpoint that is already being destroyed. Live endpoints still receive their normal notification, and entity-state guards remain enabled.
- Release cached attachment identifiers when a client critter view is finalized, preserving the live peers and the destructor's cleanup checks. Native regressions cover live detach, destruction of either endpoint, outgoing attachment notifications and exception reports.

### Migration

- Reconfigure and rebuild native clients, servers and unit tests, then rebake the embedding project's resources before validation. No project caller or data conversion is required. API, settings, network compatibility `0.0.69`, ABI, resource schemas and saves retain their current meanings; no migration rule is needed.
- Regenerate documentation snippets, translation state, site/search/routes, AI evaluation and AI delivery after integrating this update.

## 2026.1.24-dev - 2026-10-10

### Added

- Client scripts can query the desktop dimensions and display modes of the monitor containing the current window through `Game.GetDisplaySize` and `Game.GetDisplayModes`. SDL results exclude invalid and duplicate dimensions; unavailable display information returns zero dimensions or an empty list. Headless and stub windows provide the same unavailable result.

### Migration

- Upgrade from `2026.1.23-dev` at `e08ca424e4f24561d6610686b2b9f9548b9365db`. The preceding example configuration refresh is retained; no additional project settings conversion is introduced by these display queries.
- Existing script calls, settings and stored data keep their meaning; no caller or persisted-data conversion is required. Regenerate bindings and rebake scripts before using the two new queries. The caller owns its resolution bounds, standard presets and fallback when display information is unavailable.
- Rebuild Engine and embedding-project native binaries together for the additional `IAppWindow` virtual methods, and deploy matching baked metadata. Network compatibility remains `0.0.69`; network formats, resource schemas and saves do not change. Reconfigure native version/revision metadata for `2026.1.24-dev` and regenerate the API references, translation state, site/search/routes and AI delivery.

## 2026.1.23-dev - 2026-10-10

### Fixed

- Regenerate the complete MinimalMultiplayer, ContentShowcase and PackagingMatrix
  configurations after the server grid settings replacement. All three now use
  `Server.ProtoMapGridType = Dynamic` and `Server.MapInstanceGridType = Dynamic`,
  preserving their previous dynamic-grid defaults. Existing example freshness
  and package tests validate these generated inputs. See
  [generated content](../how-to/build/generated-content.md).

### Migration

- Upgrade from `2026.1.22-dev` at `bf673df1818c10aaf77c586077caf60cbd2e904f`.
  No additional project migration is required: this refreshes Engine-owned
  example configs for settings already introduced in that version. Follow its
  migration notes when upgrading from an older Engine. Native behavior, host/runtime
  ABI, network compatibility, resource schemas, map/save formats and database
  rules are unchanged; no data conversion is needed.

## 2026.1.22-dev - 2026-10-10

### Added

- Hidden `ServerMapGridMemoryDynamic` and `ServerMapGridMemoryChunked`
  measurements for empty/populated runtime maps, increasing field-write history,
  destruction and shutdown. Fresh-process comparisons report process-private
  bytes and check identical entity counts/field behavior without memory thresholds.
  See [testing](../contributing/testing/).
- Hidden `ServerMapGridOperationsCost` benchmark for production server path,
  reachability, trace, field-read and block-write operations across all nine
  dense/chunked/hash prototype and instance combinations. It checks identical
  results and reports five warmed wall/CPU samples without timing thresholds.
  See [testing](../contributing/testing/).
- Independent string selectors for shared map prototype and runtime instance
  grids: `Server.ProtoMapGridType` and `Server.MapInstanceGridType` default to
  `Dynamic` and accept exactly `Static`, `Chunked` or `Dynamic`. Unknown or empty
  values reject server startup. These select three sibling implementations;
  chunked grids do not inherit from static grids. Block side comes from
  `GameSettings::SERVER_MAP_CHUNK_SIDE` (16). See
  [server field storage](../explanation/runtime/server.md#server-map-field-storage).

### Fixed

- Replace the client's full-map field allocation with directly indexed 16x16
  dense blocks, created on first write. Large maps no longer need one contiguous
  allocation containing every field. Missing reads, scrolling, overlapping
  Mapper resize and unload preserve their behavior; light buffers remain dense.
  Add actual large-map memory regressions, block-boundary/ownership tests and
  a hidden comparison of native field-access costs. See
  [client field storage](../explanation/runtime/client.md#client-map-field-storage).
  Block side is a template parameter; `MapView` selects it through the
  compile-time `GameSettings::CLIENT_MAP_CHUNK_SIDE` constant (16).

### Migration

- Upgrade from `2026.1.21-dev` at `cfa468f17bd5dff82cebc6f05b568620fc34163c`.
  `ChunkedTwoDimensionalGrid` is a new native storage type;
  `StaticTwoDimensionalGrid` and `DynamicTwoDimensionalGrid` retain their existing
  contracts. Private `MapView::_hexField` uses the new type. Native clients,
  embedded clients and Mapper on all platforms/backends must rebuild every
  consumer of `MapView` headers because the private C++ layout changes. No
  existing hex-coordinate meaning, map format, save or database
  conversion is required. Do not mix old native objects with new headers.
- Follow the [generated-content order](../how-to/build/generated-content.md):
  configure/codegen, rebuild affected native applications and tests, bake
  matching metadata/resources, then repackage the matching host/runtime.
  Host/runtime ABI version, resource schemas, network compatibility `0.0.69`
  and database `MigrationRule` remain unchanged. Replace the published boolean
  settings `ProtoMapStaticGrid` and `MapInstanceStaticGrid` with the corresponding
  `ProtoMapGridType` and `MapInstanceGridType` strings. For each layer, the former
  static/chunk flag combination maps to `Dynamic` when static was false,
  `Static` when static was true and chunked false, and `Chunked` when both were
  true. Earlier unpublished `ProtoMapChunkedGrid`, `MapInstanceChunkedGrid`,
  `ProtoMapStaticGridChunked` and `MapInstanceStaticGridChunked` keys are removed;
  no aliases remain. Regenerate bindings and bake matching metadata. Native
  `StaticMap` callers now pass `(size, grid_type)`; `Map` construction is no longer
  `noexcept` and invalid selectors throw `SettingsException`. Rollback uses the previous complete native package;
  no database restore is needed.
- Run `ClientLargeMap*`, `TwoDimensionalGrid`, client-map/lifetime and
  `ServerMapGridSelectionPreservesFieldBehavior`, invalid-selector and
  constructor-unwind checks plus server-map/lifetime,
  sprite/atlas/roof regressions, compare repeated `ClientMapFieldAccessCost`
  runs and `ServerMapGridOperationsCost`, and qualify the embedding project's
  ordinary rendered map-entry route.
  Constructor-memory and access-cost fixtures do not establish long-session
  32-bit OOM freedom, whole-game FPS or server tick throughput. Regenerate snippets, translation state,
  site/search/routes, AI evaluation and AI delivery after integration.

## 2026.1.21-dev - 2026-10-10

### Fixed

- Write back mutable Managed event lists and dictionaries through the caller's collection accessor after converting their entries. Native collection proxies retain container-owned addresses after additions and vector growth, refresh dictionary order, and preserve duplicate-key insertion semantics and const write rejection.
- Add a reusable Engine-source native probe for boxed collection events, including GC, the outer script context, shutdown, and an identical negative control without writeback. It uses a configured GCC/Clang Ninja Managed unit build and prepared host runtime; unsupported payload/toolchain qualification remains separate.
- Store synchronous AngelScript and Managed database-document contexts as guaranteed non-null `ptr<const AnyData::Document>` borrows. Document reads, callback lifetime checks and migration behavior are unchanged.
- Explicitly discard document-migration change flags during entity and globals restore. Both callers still persist the updates output and propagate migration exceptions; remove the two Clang `nodiscard` warnings.
- Apply the owning clang-format 20 layout to AngelScript backend includes and Managed document-migration bindings; formatting CI no longer produces a diff for these files.
- Review the complete 112-document AI context after version 20 reached 2,228,855 bytes. Extend its hard limit by 32 KiB to 2,260,992 bytes; retain whole documents, migration history and the existing membership policy.

### Migration

- Rebuild native script backends and rebake affected script roles. Existing `ref List<T>` and `ref Dictionary<K, V>` event handlers now propagate their resulting collections to the caller and subsequent subscribers; no handler signature or scalar ABI changes are required.
- AI delivery consumers must honor the manifest's `full_context.max_bytes` increase from 2,228,224 to 2,260,992 bytes. Regenerate documentation outputs with `python BuildTools/docs_prepare.py` before source/freshness tests, then run the complete site/browser gates. Oversized bundles still fail instead of being truncated.
- No script/native API, runtime configuration, serialized data, network, ABI, resource schema or save conversion changes. Rebuild version metadata for this revision; documentation dependency pins and publication routes are unchanged.

## 2026.1.20-dev - 2026-10-09

### Fixed

- Wait for lazy architecture-diagram images before browser rendering checks and screenshots. Delayed SVG responses no longer cause false audit failures; missing images still fail in every profile.
- Run the real-browser delayed/missing-image regression in documentation CI.

### Migration

- No script/native API, configuration, serialized data, network, ABI, resource schema, or save conversion changes. Rebuild version metadata when adopting this revision. Browser dependency pins and the full route/profile scope are unchanged.
- After the existing pinned Chromium setup, run `npm --prefix BuildTools/docs-browser run test:diagram` and the complete browser audit. Regenerate documentation, translations, site/search/routes, and AI delivery for the new version.

## 2026.1.19-dev - 2026-10-09

### Fixed

- Normalize document migration comments.

### Migration

- Comments only; no API or save conversion. Rebuild version metadata and regenerate documentation outputs.

## 2026.1.18-dev - 2026-10-09

### Added

- Add `MigrationRule Property <Owner> Remove <Old>` as a permanent name tombstone. Skip its historical document/text value and reject a live declaration under that name, including RefType fields.
- Add AngelScript document Property/Proto Transform support with `[[PropertyMigrator]]`/`[[ProtoMigrator]]`, typed `&inout` values and `const DatabaseDocument&inout`, automatic source/bytecode validation and binding, and synchronous atomic staging. Namespace-qualified function names use `Namespace::Function`; rebake AngelScript scripts for the new registered type and callbacks. Release RefType arrays/dictionaries through their container cleanup paths.

- Add atomic Property Transform callbacks: native `PropertyMigratorCallback`, Managed C# `[PropertyMigrator]`, `PropertyMigrator<T>` and original `DatabaseDocument` contexts. Stage canonical fields before entity loading.

- Add `[ProtoMigrator]` document callbacks for Proto Transform. Resolve original-document chains before lookup, detect cycles and stage `_Proto`; ordinary lookup and baking apply only Rename/Remove.

### Changed

- Require the explicit `Rename` action for property-name rules and an existing destination. Validate that `Transform` targets an existing Persistent property; server-only dynamic owners and their rules follow metadata role filtering.

### Migration

- For a deleted property, retain `///@ MigrationRule Property Owner Remove Old` with no replacement. Rename and Transform keep their five-part forms; both Property and Proto Remove use four parts. Name reuse is forbidden regardless of the new property type.
- Replace every `///@ MigrationRule Property Owner Old New` declaration with `///@ MigrationRule Property Owner Rename Old New` in native/script source, generated consumers, tooling, and fixtures. Historical source names must point to existing live fields, including nested RefType fields. The old four-part property form is rejected; Proto rules also require an explicit action: use Rename Old New, Remove Old, or Transform Old QualifiedType.Function. The old __remove__ replacement is rejected; Enum, Entity, EntityHolder and Version retain their forms.
- For value migration, register `///@ MigrationRule Property Owner Transform Property QualifiedType.Function` and implement a static synchronous `[PropertyMigrator]` method returning `bool` with parameters `ref T` and `DatabaseDocument`, where `T` exactly matches the property's managed type. `false` keeps the original serialized value; `true` serializes the result. Do not retain the context, perform entity mutations, issue rewards, or infer the identity type from matching text. Projects must supply semantic conditions, collision policy, idempotency, and old-save/restart fixtures. Managed C# binds functions automatically; unbound callbacks refuse migration.
- Rebuild native binaries and rebake all metadata and managed roles together. Property and Proto Rename/Transform records now contain five parts; Remove contains four for either kind; the outer metadata file header is unchanged. The manual compatibility marker advances from 68 to 69, so mixed old/new executable and resource sets are unsupported. No database schema, field storage type, transport message, platform payload, or unrelated property behavior is changed; the added native callback API requires rebuilding embedding binaries.
- Database loading applies conversions to Game, Player, ordinary and custom entities, including batched loads. An exception during preparation leaves the document and update queue untouched; this guarantee covers one document, not a transaction over all world documents. Retain migration rules for the supported save horizon and verify a database copy before production adoption. Regenerate API/reference, snippet, translation, site/search, and AI delivery artifacts.

## 2026.1.17-dev - 2026-10-09

### Fixed

- Validate existing READY trees, downloaded workspace caches, prebuilt inputs and freshly published managed runtimes against the nonempty embedding headers, managed entry assemblies and target-specific link archives. Recover incomplete trees through the existing cache/republication route instead of reporting a runtime ready with missing linker inputs.

### Migration

- No script/native API, configuration-key, serialized-data, network, ABI, resource-schema or save migration is required: this change verifies build-preparation inputs and preserves the meaning of complete runtimes. Reconfigure and rebuild native version/revision metadata when adopting this Engine revision.
- Run the normal `SetupManagedRuntime` target (or BuildTools `setup-mono <os> <arch> <config>`) to recover an incomplete READY tree. Without a workspace cache, existing valid source/object markers permit republication without recompiling Mono; configured caches accept complete entries and rebuild incomplete hits. Complete trees retain their existing cache identity and marker names.
- `FO_MANAGED_RUNTIME_PREBUILT` must name a complete target tree containing the headers used by the managed backend, CoreLib and `System.Runtime.dll`, the platform's static archives, and browser JavaScript glue when applicable. Replace incomplete prebuilt input with a complete published target tree; it is rejected before copying or writing READY. Regenerate localization, site/search/routes, AI evaluation and AI delivery after updating the owning guide.

## 2026.1.16-dev - 2026-10-09

### Changed

- Move the authored Jekyll config to `Docs/Site/_config.yml`. Generate Ruby dependency/domain files and public root endpoints under ignored `Workspace/Documentation/`; render and export with `docs_site_build.py` into `Workspace/DocumentationSite/`. Public URLs are preserved.

- Generate documentation delivery indexes and validation catalogs at build time instead of committing them. This includes `docs-manifest.json`, both `llms` endpoints, site navigation/search, and six generated reports. `AGENTS.md` remains the source-checkout maintainer entry point; the website retains its public retrieval URLs.
- Prepare documentation outputs before CI validation and site rendering. Publish the validated Pages artifact only after the master-push version and documentation gates pass.

### Fixed

- Apply declared Engine defaults before ordinary application configuration is loaded. Settings omitted from a project or packaged config retain their `Settings.inc` defaults; explicit config, sub-config, cached local-config and command-line values keep their existing precedence, including explicit zeroes.

### Migration

- Replace bare Jekyll commands with `python BuildTools/docs_site_build.py`; install dependencies using `bundle install --gemfile Workspace/Documentation/Gemfile` after preparation. `Gemfile`, `.ruby-version` and `CNAME` are derived from the source manifest, with no versioned root copies.

- Run `python BuildTools/docs_prepare.py` before local documentation checks or Jekyll. Public contract models, Markdown, policies, translations and SVG/PNG remain versioned and must still pass their freshness checks. Do not stage the ignored delivery outputs.
- Before the first master push with this change, select GitHub Actions as the repository's Pages build source; the former branch build cannot generate ignored files. Verify the first deployed artifact and existing public endpoints. The worktree change does not switch remote settings or deploy a site.
- Reconfigure and rebuild affected native applications, then bake and validate the embedding project's startup profiles. Review settings previously omitted from configuration: they now receive their declared defaults instead of value-initialized zeroes. No setting name or declared default changes, and explicit values retain their meaning.
- Native/script API, network compatibility `0.0.68`, ABI, resource schemas and saved data are unchanged. No project-source or persisted-data conversion is required.

## 2026.1.15-dev - 2026-10-09

### Changed

- Consolidate route and translation reports in `docs-manifest.json`. Navigation and AI delivery reference `VERSION`; the Jekyll build reads its value rather than committing version copies. Search excludes technical HTML comments, and the AI full-context bundle leaves document hashes in the public index.

### Migration

- Documentation consumers must replace `Docs/generated/document-routes.json` with `docs-manifest.json#/routing` and `Docs/generated/translation-status.json` with `docs-manifest.json#/translation_status`. These are JSON sections of one file, not filesystem paths. AI delivery schema is `3`; site delivery schema is `4`; report field meanings and translation review hashes are preserved. The obsolete standalone reports are removed.
- Regenerate localization, site/search/routes, AI evaluation when affected, then AI delivery. The Jekyll layout includes `VERSION` directly; retain `includes_dir: .` in `_config.yml` for local and GitHub Pages builds. Publish root `VERSION` verbatim. No native/script API, configuration, serialized data, network, ABI, resource-schema or save migration is required.
- Reconfigure and rebuild native version/revision metadata when adopting this Engine revision.

## 2026.1.14-dev - 2026-10-08

### Fixed

- Recover model picking after an SDL_GPU readback submission or wait failure. Failed requests report an error without exposing unconfirmed pixels; model alpha masks discard the failed reader and retry while preserving the last completed mask.

### Migration

- No configuration, caller, serialized data, network or resource-format migration is required. Rebuild the client to adopt the readback recovery. Existing alpha thresholds and successful readback results retain their meanings; network compatibility, ABI, resource schemas and saves are unchanged. Reconfigure and rebuild native version/revision metadata when adopting this Engine revision, then regenerate translation state, site/search/routes, AI evaluation and AI delivery.

## 2026.1.13-dev - 2026-10-06

### Fixed

- Distinguish managed compilation staging from published assembly resources in the Managed C# guide and startup troubleshooting. The baker, packager and backend use the lowercase role directories documented here.

### Migration

- No project caller, configuration or data conversion is required: this update corrects documentation of existing resource locations. Keep published DLLs in `Assemblies/Assemblies-<target>/` (`server`, `client`, `mapper`); `Assemblies/<Target>Assemblies/` holds MSBuild intermediates. Existing packages remain valid. API, settings, network compatibility `0.0.68`, ABI, resource schemas and saves retain their current meanings. Reconfigure and rebuild native version/revision metadata when adopting `2026.1.13-dev`.
- Regenerate documentation snippets, translation state, site/search/routes, AI evaluation and AI delivery after integrating the corrected guide and this dated entry.

## 2026.1.12-dev - 2026-10-06

### Fixed

- Update the macro-only generated-configuration regression to check the Engine version and revision introduced by the versioning contract. Use fixed fixture identities so the test does not depend on the current checkout.
- Run the existing codegen/default-argument regressions in the `engine-update` CI job with an explicit pytest dependency.
- Shorten the private test-cache explanation to the Engine comment standard; cache ownership and cleanup are unchanged.

### Migration

- No project caller or data conversion is required. Generated macros retain their current names, values and ordering; API, settings, network compatibility `0.0.68`, ABI, resource schemas, saves and package behavior are unchanged. Reconfigure and rebuild native version/revision metadata for this Engine update.
- The Ubuntu `engine-update` job installs `python3-pytest` and executes `python3 -m pytest -q BuildTools/tests/test_codegen_default_args.py` before publication-range validation. Local source-only checks need pytest; native and embedding-project validation remain required for runtime changes. Regenerate documentation snippets, translation state, site/search/routes and AI delivery when integrating this revision.

## 2026.1.11-dev - 2026-10-06

### Changed

- Managed script projects, the managed host and Engine-owned C# tools compile with integer overflow
  checking enabled. Runtime fragments and live patches use the same default. Overflowing arithmetic
  and numeric narrowing now raise `OverflowException`; representable results retain their meaning.
- `ucolor` packs masked color components in unsigned arithmetic, preserving all existing low-byte
  values with overflow checking enabled.

### Migration

- The complete checked-arithmetic guide and migration notes fit within the existing reviewed
  full-context limit of 2,228,224 bytes. AI delivery consumers must honor the declared
  `full_context.max_bytes`; whole-document inclusion and fail-closed validation remain required.
- Rebuild the managed baker, regenerate the host and Server/Client/Mapper projects, compile every
  enabled managed role, rebake resources and package the rebuilt assemblies. Rebuild copied C# tool
  projects with `CheckForOverflowUnderflow=true`; the Engine projects already declare it. Runtime
  fragments and patches compiled after the update inherit checked arithmetic automatically.
- Review intentional integer wrapping and signed/unsigned bit reinterpretations. Search C# sources
  for hash multiplications and casts of packed RGBA values; use expression-local `unchecked` only
  where wrapping or bit truncation is the algorithm's contract. For example, change a wrapping hash
  assignment `hash *= prime` to `hash = unchecked(hash * prime)`, and a packed vertex-color conversion
  `(int)color.value` to `unchecked((int)color.value)`. Keep ordinary gameplay arithmetic checked.
- Verify that overflowing addition and narrowing throw `OverflowException`, explicit `unchecked`
  still wraps, and existing valid color/hash outputs remain unchanged. Floating-point NaN/infinity
  still require explicit finite-value checks; this compiler option does not reject them.
- No persisted-property, save, prototype, native ABI, wire protocol or resource-schema conversion is
  required. Numeric storage widths and runtime compatibility markers are unchanged by this update.
  Deliver the rebuilt managed assemblies with their matching native build; retain the previous
  package as the rollback boundary. No old API alias or data migration is introduced.

## 2026.1.10-dev - 2026-10-05

### Fixed

- Rebase unpublished TLA integration fixes onto published master `b1e3fa58021a171877177a19c3d26127abf486be`. Consolidate the prior merge-only changes into one local step; preserve the upstream Git-policy, version and bilingual release notes.
- Retain actual lowercase baked image resource names and frame-composition regression coverage, process-isolated test caches, the 2D/AngelScript fixture guard and ground-item visibility precondition. Client-updater fixtures probe for an unused local TCP port instead of assuming a candidate is free.
- Reconcile the owning image/baking/testing guides and generated documentation. Complete AI delivery retains the reviewed 2228224-byte bound needed by these local explanations.

### Migration

- Reconfigure and rebuild native clients, servers and version/revision metadata together. No TLA caller, setting, wire, ABI, resource schema or save conversion is introduced by this rebase; compatibility remains `0.0.68`. The natively tested source is unchanged. When upgrading from an earlier published engine, apply the preceding timestamp and movement migrations in order.
- Delivery consumers must honor the full-context `max_bytes` increase from 2195456 to 2228224 bytes against this published baseline. Whole-document membership and fail-closed budget checks remain required. Test processes use private caches; the user's local settings and mapper history remain intact.

## 2026.1.9-dev - 2026-10-05

### Fixed

- Version the published Git-policy update (`8697fcd9d9`); align governance tests and versioning guidance.

### Migration

- Fetch upstream; rebase only unpublished work, even with an upstream. Merge diverged published histories; preserve published ancestors. No source/data migration: API, settings, protocol `0.0.68`, ABI and resources stay unchanged. Reconfigure/rebuild version metadata.

## 2026.1.8-dev - 2026-10-05

### Fixed

- Add missing version/notes after README badge removal (`3904dc3e84`).

### Migration

- No source/data migration: documentation-only. Reconfigure/rebuild `2026.1.8-dev` metadata; API, settings, protocol `0.0.68`, ABI and resource schemas stay unchanged.

## 2026.1.7-dev - 2026-10-05

### Fixed

- Link-delay estimation rejects unrepresentable stamps before mutation and bounds nanosecond lateness.

### Migration

- Rebuild native clients and servers. Timestamp/settings semantics, wire layout, compatibility `0.0.68`, ABI, resources and saved data are unchanged. No caller/data migration is needed. Invalid stamps raise `Sender timestamp is outside the native clock range`; test signed-64-bit extremes and ordinary delays.

## 2026.1.6-dev - 2026-10-05

### Fixed

- Correct EN/RU movement-entry dates to 2026-10-05 UTC.

### Migration

- Rebuild version metadata; apply the movement migration below from `2026.1.4-dev`. No runtime/data changes.

## 2026.1.5-dev - 2026-10-05

### Changed

- Held-direction movement traces and extends plans, retargets on turns and can slide along obstacles. Step leases bound how far the server and observers continue when the controlling client's link stalls.
- Shared movement synchronization measures usual link transit, catches up late plans and joins plans at an already traversed step. These changes also apply to point-and-click movement; destination pathfinding remains in place. Observer catch-up can play smoothly instead of immediately jumping ahead. See [movement](../explanation/maps-and-movement.md) and [networking](../explanation/authority-and-networking/index.md).
- The build-hash regression fixture stages the real CMake helpers beside its temporary project, supporting Windows checkouts and temporary directories on different drives.

### Migration

- Upgrade the server, controlling clients and observers together. `SendCritterMoveLease` and `CritterMoveLease` extend the wire protocol; the compatibility marker advances from `0.0.66` to `0.0.68`. Reconfigure, rebuild native hosts/runtime libraries and rebake both sides from the same source revision. Do not bypass the compatibility check to run mixed revisions. No persistent property/prototype rename or save conversion is introduced by this movement change.
- Review new `Network.LinkDelayWindowMs`, `Network.LinkDelayRebaseMs`, `Network.MoveLateCatchUpMinMs`, `Network.MoveLateCatchUpMaxMs` and `Network.MovePlanJoinMaxSteps` in every affected configuration. Existing settings retain their meaning. Search configuration sources for these exact keys; absent keys use the documented defaults. A zero join bound disables joining.
- Review new `Client.DirectMoveTraceSteps`, `Client.DirectMoveExtendAheadSteps`, `Client.DirectMoveLeaseSteps`, `Client.DirectMoveLeaseRenewSteps`, `Client.DirectMoveRetargetMinMs`, `Client.DirectMoveRetargetImmediateAngle` and `Client.DirectMoveSlide`. Keep extension/renewal bounds below their trace/lease bounds. No authored movement destination or ordinary point-and-click call requires rewriting.
- Review new `Client.MoveCatchUpRate` and `Client.MoveCatchUpSmoothMaxMs` for remote playback. Regenerate script API/settings references and compile affected scripts; all fourteen additions are read-only settings, not mutable script state.
- Validate held straight movement, turns, stopping and blocked edges with controlling and observing clients, then ordinary point-and-click destinations and interruptions. Repeat with delay/jitter/stalls, checking lease expiry and late-plan reconciliation. Existing `Network.MoveSyncTrace` can collect diagnostic evidence; keep it disabled outside diagnostic runs. These checks qualify behavior separately from performance.
- When maintaining a copied build-hash test fixture, stage the three actual CMake helper files beside that fixture instead of deriving a cross-drive relative path. Production build-marker behavior is unchanged.
- Regenerate the MinimalMultiplayer, ContentShowcase and PackagingMatrix configurations with their `generate_config.py` owners so the fourteen movement settings are present. The maintained native test inventory includes `Test_LinkDelay.cpp`; bilingual documentation and snippet coverage fixtures now include the two version-policy pages.
- Documentation search consumers must honor the manifest's reviewed per-locale `max_bytes`, increased from 1,835,008 to 1,867,776 bytes for the complete movement/settings and migration corpus. Membership, token policy and fail-closed enforcement remain intact; the AI full-context limit is unchanged.

## 2026.1.4-dev - 2026-10-04

### Fixed

- POSIX crash-handler setup and default-signal re-raise compile when Darwin SDK signal-set operations are function-like macros. The five calls preserve their masks, flags and signal values; Linux retains its existing function behavior.

### Migration

- Rebuild affected macOS native targets after updating the Engine revision. No project API, setting, persisted data, ABI, protocol or resource format changes, so no caller or data migration is required.

## 2026.1.3-dev - 2026-10-04

### Fixed

- The configuration/tools documentation test counts all 194 required current translations, including the versioning and changelog pages. Exact inventory, zero missing translations and complete localization remain required.
- Both translation workflow guides report the same current page inventory as the generated localization model.

### Migration

- No project migration or data conversion is required: only documentation and its test expectations change. Game APIs, settings, persisted properties, native ABI, network compatibility and resource formats retain their existing contracts.

## 2026.1.2-dev - 2026-10-04

### Fixed

- Saved native stack-trace context storage on macOS now accommodates the SDK libunwind context. Its sixteen-byte alignment is preserved; other supported platforms keep their existing storage size.
- Native and managed source formatting and explanatory comments conform to the maintained style checks. The English and Russian debugging guides describe the platform context boundary.
- Documentation coverage tests include the new versioning and changelog pages and their normative examples while retaining exact inventory and completeness checks.

### Migration

- No game API, setting, persisted property, protocol, ABI or resource format is replaced. Existing valid inputs and runtime compatibility markers keep their meanings, so no project data migration is required.
- Reconfigure and rebuild native targets after updating the Engine revision. macOS compilation must use its platform SDK to check the saved-context size assertion; Linux tests do not qualify this macOS branch. Project versions and package/updater build hashes remain project-owned.

## 2026.1.1-dev - 2026-10-04

### Added

- Engine CalVer from `VERSION` is validated during CMake configuration, compiled into `FO_ENGINE_VERSION`, and recorded in native startup logs. `FO_ENGINE_REVISION` records the Engine SHA and tracked-file `-dirty` state separately from the embedding project's build hash.
- The documentation version link opens this changelog in the selected language. Navigation/route data and AI delivery expose the Engine version from the same source file.
- Bilingual change notes and release checks establish an owner for developer-visible changes and required migration instructions.

### Changed

- Current-only Engine API support preserves the original meaning of existing values. Replacements remove the old API and provide actionable compile/bake/validation failures plus exhaustive bilingual migration records; existing persisted-property `MigrationRule` conversion remains the narrow exception.
- Every published master step increments minor and records its UTC year/date; release lines freeze year/major/minor and advance patch, using `-rc` for stabilization. Documentation-only, CI, test and dependency updates are included.

- The historical `2022.1.0.wip` placeholder in `VERSION` is replaced by a CalVer development identifier. Existing historical API `Since` annotations retain their original values.

### Migration

- The bounded full-context delivery limit increases from 2,162,688 to 2,195,456 bytes to include the complete mandatory update/migration policy. Delivery consumers must honor the declared `full_context.max_bytes` rather than a fixed previous buffer; whole-document inclusion and fail-closed validation remain.

- The earlier draft `YYYY.M.PATCH` notation is superseded. Use `YEAR.MAJOR.MINOR-dev` on master and `YEAR.MAJOR.MINOR.PATCH[-rc]` on `release/YEAR.MAJOR`; major is the line ordinal, and the month is in the note date. Initial policy adoption sets `2026.1.1-dev`. Consumers parsing the earlier version syntax must accept these exact forms and reject obsolete suffixes/final three-field forms; `BuildTools/engine_version.py` is the shared parser. See the [complete update checklist](../how-to/release/versioning.md#every-master-update), release-cut sequence and migration record before preparing any update.

- Documentation-manifest versioning policy now uses schema version `3` and declares `engine: {scheme: calver, source: VERSION, master_format: YEAR.MAJOR.MINOR-dev, release_format: YEAR.MAJOR.MINOR.PATCH[-rc]}`. Custom consumers regenerating these artifacts must include that policy and provide the Engine `VERSION` file. Rolling documentation still follows `master`.
- Reconfigure and rebuild native binaries to obtain the Engine identifiers. Project game versions, package/updater build hashes, runtime compatibility digests, ABI versions, and resource-schema versions keep their own contracts; this change requires no data migration or resource-schema bump.

## Earlier history

This maintained log begins with the adoption of Engine versioning. Earlier work is available in the [repository history](https://github.com/cvet/fonline/commits/master/) and the [revision-bound contract-change ledger](https://github.com/cvet/fonline/blob/master/Docs/contract-change-dispositions.json). They are audit evidence, not a reconstructed series of published Engine releases.
