---
layout: default
title: Upgrade an Embedding Project
locale: en
document_id: engine-upgrade-guide
permalink: /Docs/en/how-to/migration/engine-upgrade.html
---

# Upgrade an Embedding Project

This guide provides a repeatable Engine-update procedure for a game repository. It covers source integration, generated contracts, content, saves, networking, client runtime, and documentation. It also gives the literal Git commands, the changelog and consumer-impact search recipes, batching rules for long ranges, and a plan skeleton that pairs with the completion record.

## Source paths inspected

- `AGENTS.md`
- `VERSION`
- `.gitignore`
- `BuildTools/docs_contract_diff.py`
- `BuildTools/docs_engine_version.py`
- `BuildTools/engine_version.py`
- `Docs/contract-change-dispositions.json`
- `Docs/generated`
- `Docs/en/reference/changelog.md`
- `Docs/en/how-to/release/versioning.md`
- `Docs/en/how-to/build/embedding-project.md`
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

Record the old/new Engine `VERSION` identifiers alongside the exact SHAs and review the [changelog](../../reference/changelog.md). [CalVer](../release/versioning.md) identifies the year, release line, master change and optional release patch; compatibility still requires the source/contract audit below. Historical changes predating the maintained log require the complete revision-range review.

## Engine update does not mean Engine pin policy

This page describes how to move an Engine pin safely. It does not say when a project should move it. The project owns that decision: stay on a proven revision, take every master step, or follow only the release patches of a cut line (see [Cut, stabilize and patch](../release/versioning.md#cut-stabilize-and-patch)). A newer Engine commit existing upstream never obliges the project to adopt it.

Keep the pin reproducible while the decision is pending:

- the project pins an exact Engine commit, never a branch name;
- local edits inside the Engine checkout are not part of the pin, and the build identity then carries a `-dirty` marker (`FO_ENGINE_REVISION` in the [identity table](../release/versioning.md#identities-and-consumers));
- a reusable fix found while updating belongs in the Engine repository and arrives through a later pin, not through a private patch of the submodule (see the [validation principle](../build/embedding-project.md#validation-principle)).

For how the submodule is first added and pinned, see [creating a game repository](../project/create-repository.md).

## Record the Engine identities

A project that embeds Engine as a Git submodule has three independent Engine identities. They diverge during every update, so record each one separately.

| Identity | Meaning | Command |
|---|---|---|
| Parent gitlink at `HEAD` | Pin recorded by the last project commit | `git ls-tree HEAD Engine` or `git rev-parse HEAD:Engine` |
| Index entry | Pin the next project commit would record | `git ls-files --stage Engine` or `git rev-parse :Engine` |
| Engine checkout | Commit actually built and tested | `git -C Engine rev-parse HEAD` |

Capture them as variables and compare:

```bash
old=$(git rev-parse HEAD:Engine)
staged=$(git rev-parse :Engine)
checkout=$(git -C Engine rev-parse HEAD)

echo "gitlink at HEAD: $old"
echo "index entry:     $staged"
echo "Engine checkout: $checkout"
git submodule status Engine
git -C Engine show "$old:VERSION"
git -C Engine status --short
```

```powershell
$old = git rev-parse HEAD:Engine
$staged = git rev-parse :Engine
$checkout = git -C Engine rev-parse HEAD

Write-Output "gitlink at HEAD: $old"
Write-Output "index entry:     $staged"
Write-Output "Engine checkout: $checkout"
git submodule status Engine
git -C Engine show "${old}:VERSION"
git -C Engine status --short
```

`git submodule status` prefixes the line with a space when the checkout equals the index entry, `+` when the checkout differs from it, `-` when the submodule is not initialized, and `U` on a merge conflict. `git diff --submodule=log -- Engine` lists the commits between the index entry and the checkout, and `git diff --cached --submodule=log -- Engine` lists the commits between `HEAD` and the index entry.

Use the gitlink at `HEAD` as the old revision of an update, because it is the revision the project last committed. A checkout or index entry that already differs from it is unfinished work: resolve or record it before starting, and do not take it as the baseline. When the checkout is uninitialized or behind the index entry, `git submodule update --init Engine` restores it to the index entry. Name the path so that other submodules the project may carry are not touched.

## Preserve the starting state

1. Fetch the project and Engine remotes.
2. Confirm whether either worktree has local changes.
3. Create a named safety branch and, when needed, a named stash in both repositories.
4. Record `git rev-parse HEAD`, the Engine gitlink, and remote tips.
5. Keep safety refs until the updated project validates cleanly.

Do not reset, discard, or overwrite unrelated local work to make the update look clean.

### Safety refs and stashes

```bash
git fetch origin
git -C Engine fetch origin

git status --short
git -C Engine status --short

git branch engine-update-safety
git -C Engine branch engine-update-safety "$old"
git -C Engine branch --remotes --contains "$old"

git stash push --message engine-update-safety
git -C Engine stash push --include-untracked --message engine-update-safety
```

```powershell
git fetch origin
git -C Engine fetch origin

git status --short
git -C Engine status --short

git branch engine-update-safety
git -C Engine branch engine-update-safety $old
git -C Engine branch --remotes --contains $old

git stash push --message engine-update-safety
git -C Engine stash push --include-untracked --message engine-update-safety
```

- Run the two `stash` commands only for a repository whose `status` output is not empty; Git prints `No local changes to save` otherwise.
- A stash is local to its own repository, so the project stash does not contain Engine worktree edits. Keep both.
- `git -C Engine branch --remotes --contains "$old"` prints nothing when the old pin exists only locally. The safety branch then is the only reference that keeps that commit reachable.
- List retained refs with `git branch --list "engine-update-safety"`, `git stash list` and `git -C Engine stash list`. Choose names that identify the update, and keep them until the completion record names the condition for removing them.

## Fetch, pin and stage the new revision

1. Define the target as an exact commit of the Engine repository. A published release is tagged `v<version>`; resolve the tag to a commit rather than recording the tag name.
2. Verify that the commit is present and that the old pin is its ancestor. A non-ancestor target is a different history line, not an update of this one.
3. Check out the target in the submodule, detached, and stage the new gitlink.

```bash
new='<new-engine-sha>'

git -C Engine rev-parse --verify "$new^{commit}"
git -C Engine merge-base --is-ancestor "$old" "$new" && echo "old pin is an ancestor"
git -C Engine log --oneline "$old..$new"

git -C Engine checkout --detach "$new"
git add Engine
git diff --cached --submodule=log -- Engine
```

```powershell
$new = '<new-engine-sha>'

git -C Engine rev-parse --verify "$new^{commit}"
git -C Engine merge-base --is-ancestor $old $new
if ($LASTEXITCODE -eq 0) { Write-Output 'old pin is an ancestor' }
git -C Engine log --oneline "$old..$new"

git -C Engine checkout --detach $new
git add Engine
git diff --cached --submodule=log -- Engine
```

After staging, the three identities from [Record the Engine identities](#record-the-engine-identities) should agree on the new commit, except that `HEAD` still holds the old gitlink until the project commit is made. Commit the pin together with the adoption changes (configuration, scripts, content, documentation) so that no project revision records a pin that does not build.

To abandon the attempt before committing, return the checkout and the index to the old pin:

```bash
git -C Engine checkout --detach "$old"
git restore --staged Engine
```

```powershell
git -C Engine checkout --detach $old
git restore --staged Engine
```

## Audit the complete Engine range

Inspect every commit and changed path between the old and new pins:

```bash
git -C Engine log --oneline "$old..$new"
git -C Engine diff --stat "$old..$new"
git -C Engine diff "$old..$new" -- \
  Source BuildTools ThirdParty Resources Docs Examples
```

```powershell
git -C Engine log --oneline "$old..$new"
git -C Engine diff --stat "$old..$new"
git -C Engine diff "$old..$new" -- `
  Source BuildTools ThirdParty Resources Docs Examples
```

Keep one note per commit (subject, owning area, project consequence). Two narrower listings help to sort them:

```bash
git -C Engine log --first-parent --oneline "$old..$new" -- VERSION
git -C Engine diff --stat "$old..$new" -- Docs/generated
```

```powershell
git -C Engine log --first-parent --oneline "$old..$new" -- VERSION
git -C Engine diff --stat "$old..$new" -- Docs/generated
```

The first lists the publication steps, each a first-parent commit that changed `VERSION`, and therefore the points where the update can be paused. The second shows which generated contract models changed.

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

Treat embedding-project behavior only as integration evidence. The Engine source, tests, interfaces, and generated models remain normative.

## Apply explicit migration notes

Review every dated Engine change between the old/new identifiers, oldest first. Detect affected usage by each record's exact searches and diagnostics, execute all listed transformations and intermediate data/bake steps, and record the negative old-usage plus positive unaffected/migrated-use checks. Current-only API support means an unmigrated affected use must fail at compile/bake/validation; unchanged used values keep their original meaning. A generated shape diff cannot detect semantic reinterpretation. The [exhaustive migration record](../release/versioning.md#exhaustive-migration-record) is the acceptance contract.

### Find the entries for a skipped range

The English changelog (`Docs/en/reference/changelog.md`) is the migration source. Its entries are headed `## <version> - <date>`, newest first, and every entry has a `### Migration` section; the Russian changelog carries the same versions and dates. Three heading forms exist:

- `## Current - <date>` is the first dated entry and stands for the `VERSION` value of the same revision. `BuildTools/engine_version.py` resolves it to that value; the older entries keep their exact identifiers.
- `## <version> - <date>` is an older, published master step or release patch.
- `## Unreleased` is a drafting area. Read it too when the pinned commit carries text there, but it is not a dated step.

The old revision's own entry is retained in the new changelog under its exact version (the publication validator rejects a revision that drops a preceding entry). Entries newer than the old version, up to and including the new version, are the ones to apply.

1. Read both `VERSION` values:

   ```bash
   old_version=$(git -C Engine show "$old:VERSION")
   new_version=$(git -C Engine show "$new:VERSION")
   echo "$old_version -> $new_version"
   ```

   ```powershell
   $oldVersion = git -C Engine show "${old}:VERSION"
   $newVersion = git -C Engine show "${new}:VERSION"
   Write-Output "$oldVersion -> $newVersion"
   ```

2. With the new revision checked out, list the headings with their line numbers:

   ```bash
   grep -n '^## ' Engine/Docs/en/reference/changelog.md
   ```

   ```powershell
   Select-String -Path Engine/Docs/en/reference/changelog.md -Pattern '^## '
   ```

3. Or print the incoming entries oldest first with the helpers that the Engine tooling itself uses (`changelog_releases` and `read_engine_version` in `BuildTools/engine_version.py`). Save the script from [the listing below](#incoming-notes-script) outside the Engine tree, for example in the task's scratch directory as `list_incoming_notes.py`, and run it:

   ```bash
   python3 list_incoming_notes.py Engine "$old_version"
   ```

   ```powershell
   py -3 list_incoming_notes.py Engine $oldVersion
   ```

   Any Python 3 interpreter works; on some hosts `python` still resolves to Python 2. The script prints one `<version> <date>` line per incoming entry and nothing else. It reads the checked-out changelog, so check out the new revision first.

4. Open each listed entry in order. Execute its `### Migration` section completely before the next one, including intermediate regeneration, bake and data steps, and record the result in the plan.

An old version that has no changelog entry predates the maintained log or sits on another release line. Do not guess a range: review the complete commit range from the previous section.

`BuildTools/docs_engine_version.py` is the Engine's own publication validator; it lists no ranges. From a project it can still confirm that the incoming first-parent history consists of well-formed publication steps and count them. It only reads Git and the changelogs of the Engine root it is given:

```bash
python3 Engine/BuildTools/docs_engine_version.py --root Engine --check \
  --branch master --baseline-git-ref "$old" --target-git-ref "$new" --history
```

```powershell
py -3 Engine/BuildTools/docs_engine_version.py --root Engine --check `
  --branch master --baseline-git-ref $old --target-git-ref $new --history
```

A pass prints `Engine update validation passed: <N> publication step(s)`. A failure names the first step that breaks the version or changelog rules, which marks a range that needs the complete source audit. For a release line pass `--branch release/<year>.<major>` instead of `master`.

### Incoming notes script

This script prints one `<version> <date>` line per incoming changelog entry, oldest first. It reads the changelog and `VERSION` of the Engine checkout it is given.

```python
import sys
from pathlib import Path

engine_root = Path(sys.argv[1])
old_version = sys.argv[2]

sys.path.insert(0, str(engine_root / 'BuildTools'))
from engine_version import changelog_releases, read_engine_version

new_version = read_engine_version(engine_root)
text = (engine_root / 'Docs/en/reference/changelog.md').read_text(encoding='utf-8')
releases = changelog_releases(new_version, text)
versions = [version for version, _ in releases]
if old_version not in versions:
    sys.exit(f'{old_version} has no changelog entry: review the complete revision range instead')

for version, date in reversed(releases[:versions.index(old_version)]):
    print(version, date)
```

## Search project sources for consumer impact

The changelog and the contract report name symbols, settings, options, commands and file forms. For every such name, decide whether the project uses it. Collect the names from three places: each applicable `### Migration` section (including the detection patterns it prescribes), the contract report below, and the `git diff --stat` listing of the audited range.

Write one name per line into a text file, for example `update-names.txt`, and search the project's tracked sources, excluding the Engine submodule. `git grep` does not descend into submodules, so the pathspec exclusion only documents the intent:

```bash
git grep -nIF -f update-names.txt -- . ':!Engine'
```

```powershell
git grep -nIF -f update-names.txt -- . ':!Engine'
```

The command is identical in both shells. `-F` treats each line as literal text, `-I` skips binary files, and exit status `1` means no match, which is the negative evidence for that list. Search each of these areas, because the same name appears in different forms:

- script sources of every enabled backend, including test scripts;
- native extension sources and the bridge code that includes Engine headers;
- `CMakeLists.txt`, included `.cmake` files, `CMakePresets.json` and package declarations;
- the master `.fomain`, every sub-config and CI or launch scripts that pass settings on the command line;
- authored content that names a changed setting, property or file form;
- project documentation that quotes the retired name.

Search the inputs, not generated or baked output. A name found only in a generated file points at its generator input, which the search should also have found.

Per name, record where it was found, the transformation applied, and the positive check that proves the migrated use still behaves as before. A name with no match needs the negative proof the migration record asks for: the absent search result plus, for dynamically built names, the compile, bake or validation failure that an unmigrated use would raise. Entry IDs in the contract report carry the user-visible name after their prefix (for example the option name in a `cmake.option.` ID); search for that name.

## Compare generated contracts

Every Engine revision commits its generated contract models under `Docs/generated/`. With the new revision checked out, those files are the current side of the comparison. Do not regenerate them inside the pinned submodule; the generators are Engine-maintainer tools and would modify tracked Engine files.

Run the comparator from the project root against the Engine root, with the old Engine commit as baseline:

```bash
python3 Engine/BuildTools/docs_contract_diff.py \
  --root Engine \
  --baseline-git-ref "$old" \
  --write
```

```powershell
py -3 Engine/BuildTools/docs_contract_diff.py `
  --root Engine `
  --baseline-git-ref $old `
  --write
```

All paths of this tool resolve inside `--root`, not inside the project:

- `--current-dir` defaults to `Docs/generated`, `--dispositions` defaults to `Docs/contract-change-dispositions.json`, and the reports default to `Workspace/contract-diff.json` and `Workspace/contract-diff.md`; with `--root Engine` these are `Engine/Docs/generated`, `Engine/Docs/contract-change-dispositions.json` and `Engine/Workspace/contract-diff.*`.
- A path that leaves the root is rejected with `escapes the engine root`, so the reports cannot be written into the project. Engine's `.gitignore` ignores `/Workspace`, so the files do not dirty the submodule. Copy them into the task's scratch directory afterwards.
- `--baseline-git-ref` is a Git revision of the Engine repository. `--baseline-dir` must be a directory inside the Engine root holding all seventeen model files.
- `--check` computes the result and prints only the summary line, `Contract diff <status>: <N> changes across 17 domains, ...`. Use `--write` to get the per-entry Markdown report.

What is runnable from a project and what is not:

| Item | From a project |
|---|---|
| Comparator with `--root Engine`, `--baseline-git-ref`, `--check` or `--write` | Runnable; reports the changed stable entry IDs and their classification |
| `Docs/contract-change-dispositions.json` | Engine-owned ledger. Read it for the rationale and compatibility notes of an Engine change; never add a project entry to it |
| Missing-disposition entries in the report | Engine-repository work. A project cannot satisfy them; report the gap to Engine maintainers |
| Model generators (`BuildTools/docs_*.py --write`) and the CI gate | Engine-repository only |
| `git -C Engine diff --stat "$old..$new" -- Docs/generated` | Runnable without Python; shows which models changed |

`--enforce` is an Engine merge gate. It fails when a required disposition is missing, and each ledger entry is bound to one exact pair of baseline and current contract digests of a domain. Over a multi-step range the digests and change IDs differ from those of the individual steps, so the ledger entries of those steps do not match even if every step was disposed. Run it without `--enforce` for a skipped range and read the report; use `--enforce` only for a range that is a single publication step. A baseline older than a domain's first model fails with `does not exist at baseline revision`; `--allow-missing-baseline` then reports that domain as `bootstrap` instead of comparing it, so cover it with the changelog and the commit audit.

For every change determine:

- additive, documentation-only, policy-only, or breaking;
- current stability promise;
- project code/content affected;
- migration and release-note requirement;
- minimum compatible client/server/save revision;
- whether rollback remains possible after data conversion.

Do not treat an `internal` label as proof of no project impact. It only means the Engine has not made a public compatibility promise. The report also cannot see behavior behind unchanged declarations, updater protocol, or project settings without a canonical model; those rest on the changelog and the source audit. Keep the old and new reports as update evidence.

## Batch a long range

A range of many publication steps does not have to be adopted at once. Each first-parent commit that changes `VERSION` is a published step with its own dated migration entry, so it is a valid intermediate pin; list them with the `git log --first-parent --oneline "$old..$new" -- VERSION` command above. Commits inside a merged branch are not publication steps and carry no migration entry of their own.

1. Read the migration entry of every step in the range before choosing the batches.
2. End a batch at each step whose entry requires a boundary the project must pass and verify before continuing:
   - a persisted-data conversion or a `MigrationRule` change, which needs backup, isolated restore and a rollback decision;
   - a forced rebake or resource-schema change;
   - a network compatibility marker, updater protocol or client host/runtime ABI change;
   - a removed or replaced CMake option, stage, package field, setting or script API that edits project files;
   - a breaking contract change in a public domain.
3. Inside one batch apply the entries oldest first and reconcile once: one configure, one forced bake when any entry needs it, one contract report, one documentation sweep, one plan section.
4. At the end of a batch verify against the full declared matrix, commit the pin with its adoption changes, and keep its safety ref. The next batch starts from that commit as its new `$old`.
5. Do not cross a migration boundary inside a batch to save time. Executing the records of skipped updates oldest to newest, honoring intermediate data and bake boundaries, is a requirement of the [exhaustive migration record](../release/versioning.md#exhaustive-migration-record).
6. When a batch fails and the cause is unclear, halve it: pin the middle publication step, build and test there, and continue on the failing half.

A step whose entry says no project migration is required still has to be read; it may be crossed inside a batch but is not skipped.

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
- `CompileAngelScript` and/or `CompileManagedScripts` for every scripting backend enabled by the project;
- `ForceBakeResources` when the data graph changed;
- focused content/gameplay tests;
- starter/tutorial smoke when integration mechanics changed;
- visible client scene for rendering, GUI, audio, video, input, maps, or assets;
- persistence upgrade/restore rehearsal;
- client/server compatibility and updater route;
- package contents and install/launch;
- synthetic-secret checks across baked configs, package trees, archives, logs, and signing handoff;
- documentation generators, links, locale freshness, site artifact, and AI delivery.

Map claims to [Support Matrix](../../reference/platforms/support-matrix.md). A cross-build is not device qualification, and a headless test is not visible-client evidence. The project's routine checks that run on every change are described in [quality gates](../project/quality-gates.md); an Engine update runs them as well, but they do not replace the migration-specific checks above.

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

## Update plan skeleton

Create the plan before the first checkout and keep it current; the completion record below is its final form. Adapt the sections to the project, but keep the baseline and target of both repositories, the per-commit notes and the qualification limits.

```text
Goal:
  Why the project moves the Engine pin now, and the exit criterion.

Baseline and target:
  Project repository: baseline <sha>, branch <name>, safety ref <name>
  Engine repository:  gitlink at HEAD <sha> (<VERSION>)
                      index entry <sha>
                      checkout <sha>
                      target <sha> (<VERSION>), source <remote/tag>
  Ancestry check (old is an ancestor of target): <result>
  Local changes preserved: <stash names / none>

Incoming range:
  Publication steps (first-parent, VERSION changes): <list>
  Commits audited: <count>
  Per commit:
    <sha> <subject> | owner area | project consequence | action
  Changelog entries applied oldest to newest: <version list>
  Generated contract report: <path>, status, changed entries, domains without a baseline

Project consumer analysis:
  Names searched (from migrations and contract report): <file>
  Per name: matches | transformation | positive check | negative proof

Steps:
  1. <batch or step> - boundary: <data/bake/protocol/CMake/none>
     Acceptance: <observable result>
  2. ...

Verification record:
  Configure and compile per host and target: <commands, results>
  Script compile per backend: <results>
  Forced bake and side metadata comparison: <results>
  Focused tests, smoke, visible-client checks: <results>
  Save upgrade and restore rehearsal: <results>
  Compatibility, updater and package checks: <results>
  Documentation and translation checks: <results>

Qualification limits:
  Platforms, devices and rollout steps not observed: <list, marked pending>
  Owner decisions still open: <list>

Safety refs retained until: <condition>
```

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
