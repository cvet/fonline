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
