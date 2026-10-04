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
