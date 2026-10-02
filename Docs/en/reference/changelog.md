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

## Current - 2026-10-10

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
