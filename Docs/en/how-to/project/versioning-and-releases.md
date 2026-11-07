---
layout: default
title: Version and Release a Game Project
locale: en
document_id: project-versioning-and-releases
permalink: /Docs/en/how-to/project/versioning-and-releases.html
---

# Version and Release a Game Project

A game repository carries several version-like identities that answer different questions and are owned by different parties. This guide separates them, shows how to give the game a generated, monotonic version and wire it into `Common.GameVersion`, explains which changes force a client update, and describes the tag and preflight discipline around a manual release. It states requirements and a pattern; the numbering scheme itself is project policy.

## Source paths inspected

- `BuildTools/artifact_checksums.py`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/codegen.py`
- `BuildTools/package.py`
- `BuildTools/project_version.py`
- `BuildTools/release_preflight.py`
- `BuildTools/tests/test_package_wix_installer.py`
- `Docs/en/explanation/runtime/client-updater.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/release/backup-and-recovery.md`
- `Docs/en/how-to/release/operations.md`
- `Docs/en/how-to/release/packaging.md`
- `Docs/en/how-to/release/security-and-secrets.md`
- `Docs/en/how-to/release/versioning.md`
- `Docs/en/reference/cmake-and-buildtools/pipeline.md`
- `Docs/en/reference/metadata/index.md`
- `Docs/en/reference/script-api/settings.md`
- `Docs/en/reference/settings/configuration-and-data-sources.md`
- `Source/Common/Common.h`
- `Source/Common/Settings.cpp`
- `Source/Common/Settings.inc`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Tests/Test_Settings.cpp`

## Keep the identities apart

| Identity | Owner | Where it lives | What it answers |
|---|---|---|---|
| Project version | Game | `Common.GameVersion` in the main `.fomain` | Which game release is this? Shown in the startup log, the server banner and window title, and used as the Windows installer `ProductVersion` |
| Engine release identity | Engine | `Engine/VERSION` (CalVer `YEAR.MAJOR.MINOR[-dev]`) as `FO_ENGINE_VERSION`, plus the exact SHA as `FO_ENGINE_REVISION` | Which Engine source is compiled in? Used for evidence and diagnosis, never for compatibility |
| Build hash | Build | `FO_BUILD_HASH`, the game repository `HEAD` (random suffix outside Git) | Which game commit built this binary and bake? Used by packages, the updater and baker markers |
| Runtime compatibility | Generated | `FO_COMPATIBILITY_VERSION`, exposed as `Network.CompatibilityVersion` | Can this client play on this server? Gates gameplay and triggers native self-update |
| Metadata version | Generated | Computed by `MetadataBaker` from every parsed metadata tag of one bake | Were server and client resources baked from one metadata set? |
| Updater generation and host ABI | Engine | `FO_UPDATER_VERSION`, `FO_CLIENT_RUNTIME_HOST_ABI_VERSION` | Can an installed client host talk to this server and load this runtime? |
| Data migration | Game and Engine | `///@ MigrationRule` declarations; the Engine's central `MigrationRule Version` marker | Can persisted data from older releases load? |
| Package and updater identity | Game | `DefinePackage` IDs, `FO_BINARY_OUTPUT_POSTFIX`/`POSTFIX`, `Packaging.MsiUpgradeCode`, `Android.VersionCode` | Which artifact is this, and does an installer or store treat it as an upgrade of the previous one? |

Consequences that surprise teams:

- `Engine/VERSION` and the game's `VERSION` file are different files with different owners. Engine documents its own file in [Engine Versioning and Release Notes](../release/versioning.md); the game's file feeds only `Common.GameVersion`.
- Updating the Engine pin changes the Engine release identity and may change compatibility, but the Engine version number itself never decides compatibility. Read the [changelog](../../reference/changelog.md) migration notes for every skipped entry; each states whether the wire protocol, `MigrationRule Version`, Managed ABI or resource schema moved. The procedure is in [Upgrade an Embedding Project](../migration/engine-upgrade.md).
- You do not bump `FO_COMPATIBILITY_VERSION` by hand. Codegen computes it from declared metadata. The one manually edited marker, `MigrationRule Version` in `Source/Common/Common.h`, belongs to Engine.
- The Windows installer upgrade code, `Packaging.MsiUpgradeCode`, must never change once an installer has shipped. It is package identity, not version.

## Give the game a generated version

The Engine does not choose your numbering. A scheme is acceptable when it meets these requirements:

1. **Monotonic.** Every released version sorts strictly after every earlier one, under the comparison that your tags, installer and tooling use.
2. **Unique.** No two released artifacts share a version, including re-runs and rebuilds of the same commit.
3. **Derived from source-control state.** The value follows from the commit being released, so any machine reproduces it from that commit.
4. **Machine-written.** A generator writes the file; nobody edits it by hand, apart from the one seed value that bootstraps it.
5. **Checked in CI.** A job fails when the committed or generated value disagrees with the generator, is malformed, or does not exceed the latest release.
6. **Parseable by every consumer.** If the Windows `Wix` pack is used, `package.py` requires a numeric `Common.GameVersion` of two to four dot-separated integers and fails otherwise. Windows Installer also limits the numeric range of each field, so check the range of your counter against the installer documentation before placing a large counter in a leading field.

One common derivation is a fixed `major.minor` prefix plus a counter of commits on the release branch:

```bash
git rev-list --count --first-parent HEAD
```

This counts first-parent commits and so stays monotonic as long as published history only moves forward. It breaks in three ways you must prevent: rewriting published history lowers the count, a shallow clone reports only the fetched depth, and a squash-versus-merge policy change alters how many commits one change adds. Fetch full history in the version job, never rewrite published commits, and define whether the number counts the commit that contains the version file or its parent, using the same rule in the generator and in its CI check. A CI run number is monotonic per workflow but is not derived from the commit, so it fails requirement 3.

Decide where the generated file lives:

| Choice | Benefit | Cost |
|---|---|---|
| Commit `VERSION`, updated by the generator in the release or merge flow | The version is visible in source and reproducible from the commit; CI compares the file with the generator output | An extra commit or a pre-merge step, and the counting rule above must be exact |
| Do not commit it; generate before configure and bake, and ignore the file | No extra commits and no off-by-one rule | Every build path needs the generator step; a checkout without the file falls back to an unresolved value |

For the committed choice the Engine ships `BuildTools/project_version.py`. It advances `VERSION` against a base ref unless the file is already modified, checks it against the reachable history with `--check`, and takes the numbering scheme, digit limits and base ref as parameters, so the encoding stays project policy; see [Release Tooling for a Game Project](release-tooling.md#generate-and-check-the-project-version). The uncommitted choice needs a project-owned generator step, and any scheme the built-in ones cannot express is a project scheme module with its own tests.

## Wire the version into the game

1. Create `VERSION` next to the main `.fomain`. It holds one line, for example `1.4.317`. Surrounding whitespace is trimmed on read.
2. Reference it from the main configuration:

   ```ini
   Common.GameName = My Game
   Common.GameVersion = $FILE{VERSION}
   ```

   `$FILE{path}` is resolved while the authored config is read, relative to the directory of the owning config unless the path is absolute. [Configure a Game Project](../build/project-configuration.md) owns the syntax and precedence, and [Security and Secrets](../release/security-and-secrets.md#choose-the-right-configuration-form) lists `$FILE` as the right form for non-secret generated metadata such as a version. Never use it for a credential.
3. Because `$FILE` reads on the baking host, the concrete version enters the baked configuration. Generate the file before configure and bake, and use `ForceBakeResources` for a release candidate as [Packaging and Release](../release/packaging.md) prescribes. Incremental baking was not verified to re-read a changed `VERSION` file on its own.
4. A missing file is not an error. The reader logs a warning and keeps the `$FILE{...}` text unresolved, and an unresolved text is a bad version. Make your CI fail when the file is absent or the resolved value does not match.
5. Packaging resolves the same indirection itself. For the Windows `Wix` pack, `package.py` reads the file relative to the main config and asserts that it exists and is numeric.
6. Observe the result at startup. The log prints the project version and the Engine identity on separate lines:

   ```text
   Version: 1.4.317
   Engine: 2026.1.112-dev (revision 8b32281d50548593b81912d24d7625a145c8f277)
   ```

   Keep both lines in the release evidence next to the build hash.
7. If you ship Android, `Android.VersionCode` is a separate integer setting consumed by Android packaging. Keep it monotonic too, and derive it in the same generator so the two numbers cannot drift. Installer and store versions remain project-owned.

## Decide whether a change needs a client update

The compatibility digest is computed during native code generation from Engine and project C++ metadata (`///@` tags, enums, entity and type declarations, settings declarations, migration rules). It does not hash script bodies, content, prototypes or resources. Script-declared metadata travels in baked resources and has its own metadata version. Use this table to classify a release:

| Change | Identity that moves | Player effect |
|---|---|---|
| Script logic, prototypes, maps, text, art, audio, settings values | Build hash and resources | The updater delivers new resources on connect. No new client package |
| Script-declared properties, remote calls, events, enums or settings declarations | Metadata version | Server and clients must run resources from one bake. Deploy the server and the client resources it serves together; a mismatch is a deployment defect, not a supported mode |
| Project native `///@` declarations | Compatibility digest | Desktop clients (Windows, Linux, macOS) stage the new runtime and apply it at next launch. Web, Android and iOS clients see "Client outdated, please update via your app store" and need a new package |
| Project native code with no hashed declaration change | Build hash only | The digest does not change, so the compatibility verdict does not request a native update. Ship a full client package, or prove the real update behavior on a test server before relying on self-update |
| New Engine pin | Per changelog entry | Follow the migration record. A changed updater generation or host ABI requires every installed client to reinstall a full package |
| Persisted entity or property layout | Data migration | Needs a migration rule and a backup-and-restore rehearsal; see [data-migrations.md](data-migrations.md) |

Authority for these rules is [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md) (handshake, self-update platforms, release acceptance matrix) and [Generated API and Metadata](../../reference/metadata/index.md#metadata-version). When a release crosses a compatibility boundary, plan a staged rollout and keep rollback and support instructions for players on the old client; the acceptance matrix in the updater page lists the evidence for each lane.

## Tag the release

- Tag the exact commit that produced the artifacts. Name the tag from the project version, for example `v1.4.317`; Engine's own `v<version>` convention is a reasonable model.
- Make tags immutable. Never move or delete a published tag; a bad release is superseded by a higher version.
- Prefer annotated tags created by the release job, so the tag records who or what published it.
- A push of a tag that already exists with a different target is rejected by Git, which gives the final guard against a race between two dispatches. Also use workflow concurrency control so only one release job runs per project.
- Record next to the tag: project commit SHA, project version, Engine SHA and `VERSION`, the changelog range reviewed, package IDs, and artifact hashes. [Packaging and Release](../release/packaging.md#prepare-a-release-owned-package-matrix) lists the evidence fields. `BuildTools/artifact_checksums.py` writes the hashes of the final files as `.sha256` sidecars and a manifest.

## Preflight a manual release

A release is a manual dispatch from a protected ref, as described in [Dispatch releases manually](continuous-integration.md#dispatch-releases-manually). `BuildTools/release_preflight.py` carries the version checks of the preflight step. It rejects a shallow clone and tracked changes, validates the version file with the project's scheme, rejects a tag that already exists locally or on the remote, requires the version to exceed the newest release tag, and treats every ref other than the release ref as a dry run:

```bash
python3 Engine/BuildTools/project_version.py --check
python3 Engine/BuildTools/release_preflight.py --ref "$GITHUB_REF" --release-ref refs/heads/main \
  --remote origin --engine Engine
```

```powershell
py -3 Engine/BuildTools/project_version.py --check
py -3 Engine/BuildTools/release_preflight.py --ref refs/heads/main --release-ref refs/heads/main `
  --remote origin --engine Engine
```

[Release Tooling for a Game Project](release-tooling.md#preflight-a-release) documents the output, the tag prefix and scheme options, the splitting of oversized assets with a checksum manifest, and the CI wiring.

Add the checks specific to a game around it:

- The dispatched revision is on the protected release branch, or the run is a dry run that stops before publication. `--release-ref` decides the mode; keep the publish step behind the `publish=true` output.
- The `Engine` gitlink equals the pin recorded for the release, and that SHA is reachable from the published Engine branch; `--engine Engine` checks the reachability, and the clean-tree check rejects a checkout that differs from the gitlink. Do not fetch a floating Engine branch in the release job.
- The pinned lane is green for this commit, and the generated version equals the value the build will embed.
- Bake with `ForceBakeResources`, build and package from the dispatched commit, then create the tag only after artifact acceptance, as the last step before publication.

## Operate, back up and roll back

Versioning ends where operations begin:

- [Release Operations](../release/operations.md#rollout-decision) owns rollout, readiness, verification and [rollback](../release/operations.md#roll-back).
- [Backup and Recovery](../release/backup-and-recovery.md#take-a-quiesced-backup) owns the pre-release backup and restore rehearsal. A release that changes persisted data needs a verified restore before rollout.
- [Packaging and Release](../release/packaging.md) owns the package matrix, signing and acceptance evidence.
- Rolling back means redeploying the previous immutable artifact and its matching resources. Do not decrease the project version, and do not reuse a tag. Fix forward with a higher version.
- A rollback across a compatibility boundary must be rehearsed with clients that already updated; use the updater's "Compatibility break" acceptance row.

## Common mistakes

| Mistake | Effect | Fix |
|---|---|---|
| Hand-editing the version file | Duplicates, gaps and out-of-order values | Generate it and fail CI when it differs |
| Counting commits in a shallow checkout | A lower number than the real history | Fetch full history in version and release jobs |
| Treating `Engine/VERSION` as the game version | Wrong identity in logs, installers and tags | Use `Common.GameVersion` for the game |
| Tagging before artifact acceptance | A published tag for an unqualified build | Tag last, then publish |
| Moving a tag after a bad release | Two different artifacts share one identity | Issue a new, higher version |
| Mixed server and client resources | Metadata mismatch at connect | Serve client resources from the same bake as the server |

## Validation checklist

1. The version file is machine-written, and CI compares it with the generator and the latest release.
2. `Common.GameVersion` resolves through `$FILE{VERSION}`, and the startup log shows the expected project version and Engine revision.
3. The version is numeric where the Windows `Wix` pack is used, and `Android.VersionCode` is monotonic where Android is shipped.
4. The release job fetches full history, checks tag uniqueness against the remote and pins the Engine SHA.
5. The release commit, tag, artifacts, hashes, Engine SHA and `VERSION` are recorded together.
6. The update-impact table was applied: resources only, metadata, native declarations or Engine pin.
7. A rollback to the previous immutable artifact was rehearsed before the release that crosses a compatibility boundary.
8. Release dispatch has a preflight and a dry run on non-release refs.
