---
layout: default
title: Upgrade an Embedding Project
locale: en
document_id: engine-upgrade-guide
permalink: /Docs/en/how-to/migration/engine-upgrade.html
---

# Upgrade an Embedding Project

This guide provides a repeatable Engine-update procedure for a game repository. It covers source integration, generated contracts, content, saves, networking, client runtime, and documentation.

## Source paths inspected

- `AGENTS.md`
- `BuildTools/docs_contract_diff.py`
- `Docs/en/contributing/contract-change-management.md`
- `Docs/en/explanation/runtime/client-updater.md`
- `Docs/en/explanation/persistence/index.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/reference/platforms/support-matrix.md`
- `Docs/en/contributing/documentation/index.md`

## Define the update

Record before touching the submodule or vendored Engine checkout:

- embedding-project root revision;
- old exact Engine revision;
- intended new exact Engine revision;
- upstream branch/repository;
- supported build/runtime matrix;
- safety branch or stash names;
- owner of compatibility, persistence, release, and documentation review.

An Engine update is not a pointer-only change. The complete incoming Engine range and the project changes made to adopt it form one review unit.

## Preserve the starting state

1. Fetch the project and Engine remotes.
2. Confirm whether either worktree has local changes.
3. Create a named safety branch and, when needed, a named stash in both repositories.
4. Record `git rev-parse HEAD`, the Engine gitlink, and remote tips.
5. Keep safety refs until the updated project validates cleanly.

Do not reset, discard, or overwrite unrelated local work to make the update look clean.

## Audit the complete Engine range

Inspect every commit and changed path between the old and new pins:

```bash
git -C Engine log --oneline <old>..<new>
git -C Engine diff --stat <old>..<new>
git -C Engine diff <old>..<new> -- \
  Source BuildTools ThirdParty Resources Docs Examples
```

Classify changes by owner and consequence:

| Change | Required review |
|---|---|
| CMake option/stage/application/package | Project configure, target, CI, package, and support-matrix impact |
| Project library helper/core-role graph | Project dependency targets, role assignment, native bridge, platform gates, and runtime package impact |
| Setting/default/config parser | `.fomain`, sub-config, secret, resource-pack, and launch impact |
| Script API/metadata/property | Compile, bake, network, persistence, migration, and gameplay impact |
| Baker/file format/resource runtime | Authored content, forced rebake, cache/output schema, and platform impact |
| Networking/updater/client runtime | Protocol, gameplay compatibility, host/runtime ABI, package, and rollout impact |
| Database/entity serialization | Save migration, backup/restore, rollback, and mixed-version prohibition |
| Tool/editor | Authoring workflow, round trip, generated files, and screenshots/manual impact |
| Documentation/example | Engine/project ownership, links, commands, pins, and translation freshness |

Use Last Frontier or TLA only as integration evidence. The Engine source, tests, interfaces, and generated models remain normative.

## Compare generated contracts

Generate the new Engine models, then compare the old revision:

```bash
python Engine/BuildTools/docs_contract_diff.py \
  --root Engine \
  --baseline-git-ref <old-engine-revision> \
  --current-dir Docs/generated \
  --dispositions Docs/contract-change-dispositions.json \
  --write \
  --enforce
```

For every change determine:

- additive, documentation-only, policy-only, or breaking;
- current stability promise;
- project code/content affected;
- migration and release-note requirement;
- minimum compatible client/server/save revision;
- whether rollback remains possible after data conversion.

Do not treat an `internal` label as proof of no project impact. It only means the Engine has not made a public compatibility promise.

## Reconcile project configuration

Compare the project's CMake root and `.fomain` against:

- generated [CMake reference](../../reference/cmake/index.md);
- [project-local dependency guide](../../../ProjectDependencies.md);
- generated [settings reference](../../../generated/api/settings.md);
- [project configuration guide](../build/project-configuration.md);
- [security and secrets guide](../release/security-and-secrets.md);
- changed BuildTools validation/package interfaces.

Remove retired options and targets, add required values explicitly, review defaults, and test every sub-config used by CI, development, staging, and production. Re-audit `$ENV`/`$FILE` versus `$TARGET_ENV`/`$TARGET_FILE`, command-line masking tokens, side-specific baked configs, package signing handoff, and secret-bearing CI jobs. A project config should record deliberate product choices instead of inheriting a new default accidentally.

## Rebuild generated and baked data

Follow [Generated Content Workflow](../build/generated-content.md) in dependency order:

1. fresh configure/code generation;
2. native compile;
3. script compile;
4. forced resource bake when contracts or pack inputs changed;
5. side-specific metadata comparison;
6. project-generated references and snippet inventory;
7. localization status;
8. site routes, navigation, and search;
9. AI evaluation and delivery artifacts.

Keep old and new generated contract reports as update evidence. Do not hand-edit generated source or baked output.

## Protect persisted state

Before testing against valuable data:

1. create and verify a backup;
2. rehearse restore into an isolated database;
3. identify property/prototype/version migration rules;
4. test the upgrade on a representative copy;
5. verify entity counts, ownership, critical fields, and login/loading paths;
6. decide whether the migration is reversible;
7. prohibit old binaries from opening converted data when rollback is unsafe.

Property and prototype rename/remove rules are runtime contracts, not cleanup conveniences. Update references in authored content and scripts, keep migration rules for the supported save horizon, and test missing/legacy values.

Execute the provider-neutral procedure in [Backup and Recovery](../release/backup-and-recovery.md). The Engine database abstraction does not choose a game's provider, schedule, retention, schema rollout, RPO/RTO, or disaster-recovery authority; keep those concrete decisions and evidence in project operations documentation.

## Protect network and client compatibility

Review three independent boundaries:

- gameplay `CompatibilityVersion`;
- updater protocol generation;
- frozen client host/runtime ABI.

Do not assume one version covers the others. A protocol/ABI break can require a full client package and manual reinstall even when resources can self-update. A gameplay compatibility change can reject mixed client/server revisions without changing updater wire format.

For an online rollout define:

1. accepted old client cohort;
2. resource/native update path;
3. server deployment order;
4. reconnect behavior;
5. rollback point;
6. user-facing recovery for incompatible frozen hosts;
7. monitoring for update, login, sync, and migration failures.

Use [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md) for the exact current host/runtime and updater boundary. Execute the deployment, readiness, graceful-stop, and rollback sequence through [Release Operations](../release/operations.md).

## Validate the adoption

Run the narrowest checks first, then the full declared project matrix:

- Engine unit tests for changed native domains;
- configure and compile with each supported host compiler;
- `CompileAngelScript`;
- `ForceBakeResources` when the data graph changed;
- focused content/gameplay tests;
- starter/tutorial smoke when integration mechanics changed;
- visible client scene for rendering, GUI, audio, video, input, maps, or assets;
- persistence upgrade/restore rehearsal;
- client/server compatibility and updater route;
- package contents and install/launch;
- synthetic-secret checks across baked configs, package trees, archives, logs, and signing handoff;
- documentation generators, links, locale freshness, site artifact, and AI delivery.

Map claims to [Support Matrix](../../reference/platforms/support-matrix.md). A cross-build is not device qualification, and a headless test is not visible-client evidence.

## Update documentation in the same work

Reconcile:

- reusable behavior in `Engine/Docs/`;
- project integration and product policy in the embedding project's docs;
- `AGENTS.md` routing when ownership or required procedure changed;
- public examples and exact Engine pins;
- generated API/format/settings/package references;
- support matrix and platform guides;
- English source pages and every existing translation whose source hash changed;
- active plan, update record, and verification report.

The project documentation may link to Engine mechanics but should not duplicate them. Engine docs must not use a private game repository as normative proof.

## Completion record

An update record should contain:

```text
Project old/new:
Engine old/new:
Incoming Engine commits audited:
Generated contract report:
Required dispositions:
Configuration changes:
Content/resource migrations:
Save migration and restore evidence:
Network/updater/ABI decision:
Validated host/target matrix:
Visible/device checks:
Documentation and translation status:
Known residual risks:
Safety refs retained until:
```

Do not call the update complete while required evidence is missing. Record an unobserved platform or owner-gated deployment as pending rather than inferring success from adjacent checks.
