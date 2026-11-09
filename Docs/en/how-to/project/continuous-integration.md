---
layout: default
title: Set Up Continuous Integration for a Game Project
locale: en
document_id: project-continuous-integration
permalink: /Docs/en/how-to/project/continuous-integration.html
---

# Set Up Continuous Integration for a Game Project

This guide shows how an embedding game project structures continuous integration around the Engine: which lanes to run, how to check out the Engine submodule, how to prepare a runner with Engine-owned commands, how to order and gate jobs, and how to keep secrets and evidence safe. It teaches a pattern; the concrete runner labels, presets, package ids and deployment targets belong to the project.

The Engine ships two kinds of CI evidence: its own validation workflow and a pair of workflow templates for public example repositories. Both are references for structure, not drop-in release pipelines. Workflow vocabulary (`if:`, `permissions`, `concurrency`, `fail-fast`, artifacts) follows GitHub Actions because that is what the Engine's workflows use; the structure transfers to other runners.

## Source paths inspected

- `.github/workflows/validate.yml`
- `Examples/PublicRepositoryTemplate/.github/workflows/pinned-engine.template.yml`
- `Examples/PublicRepositoryTemplate/.github/workflows/current-engine.template.yml`
- `Examples/PublicRepositoryTemplate/collect-evidence.py`
- `Examples/PublicRepositoryTemplate/.gitattributes`
- `BuildTools/README.md`
- `BuildTools/buildtools.py`
- `BuildTools/prepare-workspace.sh`
- `BuildTools/prepare-win-workspace.ps1`
- `BuildTools/build.sh`
- `BuildTools/build.cmd`
- `BuildTools/validate.sh`
- `BuildTools/validate.cmd`
- `BuildTools/gameplay_test_runner.py`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `Docs/en/how-to/build/index.md`
- `Docs/en/how-to/build/embedding-project.md`
- `Docs/en/how-to/build/public-example-repositories.md`
- `Docs/en/how-to/release/packaging.md`
- `Docs/en/how-to/release/security-and-secrets.md`
- `Docs/en/how-to/testing/gameplay-and-integration.md`
- `Docs/en/reference/buildtools/commands.md`
- `Docs/en/reference/platforms/support-matrix.md`

## Run two lanes

A project needs two independent answers, and one workflow cannot give both.

| Lane | Engine revision | Trigger | Answers | Must not do |
|---|---|---|---|---|
| Pinned-Engine | The gitlink committed in the project | Every pull request and every protected-branch update | Is this project revision correct against the Engine it ships with? | Fetch a moving Engine branch |
| Current-Engine | Engine `master`, selected after checkout | Weekly schedule and manual dispatch | Is the next Engine update ready to adopt? | Rewrite the gitlink, tags, generated files or release artifacts |

The public example templates implement exactly this split. The pinned lane verifies the repository contract and the exact Engine pin before running the primary check. The current lane first verifies the same pin, then switches the submodule checkout:

```bash
git -C Engine fetch --depth=1 origin master
git -C Engine checkout --detach FETCH_HEAD
```

The switch happens after the pin check, so a drifted gitlink fails as a pin error instead of being masked by the newer checkout. Record the tested Engine commit in the lane's evidence. A green current-Engine run may justify an Engine-update pull request; a red one stays visible and is routed to the owner of the changed boundary. Follow the [Engine upgrade guide](../migration/engine-upgrade.md) for the update itself.

Release artifacts always come from the pinned lane. See [CI lanes](../build/public-example-repositories.md#ci-lanes) for the same rule in the example registry.

## Check out the repository

Check out the project and the Engine submodule explicitly, and verify that the checked-out Engine equals the committed gitlink.

```bash
git submodule update --init Engine
git submodule status Engine
git ls-tree HEAD Engine
```

`git submodule status` prefixes the line with `+` when the working tree differs from the gitlink and with `-` when the submodule is not initialized. Fail the job on either.

Rules for the checkout step:

- Name the Engine path in the submodule command. A project that also carries private or large submodules must not use an unscoped recursive update: it would fetch repositories the lane does not need and may require credentials the lane should not hold. The example templates use `submodules: recursive` because the Engine is their only submodule; that setting is not safe to copy into a project with others.
- The Engine repository declares no nested submodules and does not use Git LFS at this revision, so initializing only `Engine` is complete.
- Fetch enough history for the checks that need it. Checks that compare against a base revision need full history; the Engine's own update-validation and documentation jobs check out with `fetch-depth: 0`.
- Use the project's line-ending policy consistently on Windows and Linux runners. The example template ships a shared `.gitattributes` (`* text=auto eol=lf`, with `.cmd` and `.bat` files as CRLF).
- A persistent self-hosted Windows runner can keep stale directory spellings after a case-only rename. `buildtools.py repair-checkout-case` realigns working-tree names with the git index, and its `--check` form fails instead of renaming.

### Order Git LFS correctly

The Engine does not own Git LFS behavior; this is generic Git guidance for projects that store binaries in LFS, possibly on a non-default LFS server.

1. Check out without downloading LFS objects, so the checkout step cannot contact the wrong endpoint.
2. Make sure the committed `.lfsconfig` (or an equivalent `lfs.url` setting) and the credentials for that server are in place.
3. Fetch and materialize the objects explicitly.

```bash
export GIT_LFS_SKIP_SMUDGE=1
git checkout "$REVISION"
unset GIT_LFS_SKIP_SMUDGE
git lfs fetch origin "$REVISION"
git lfs checkout
```

`REVISION` is the commit the job tests. Fetching before the endpoint and credentials are configured fails or contacts the wrong host. Check that no pointer files remain in the tree before the bake step; a baker that reads a pointer file bakes garbage.

## Prepare the runner

Use the Engine-owned workspace commands from the checked-out revision. They are the versioned platform contract, so do not copy an apt package list or an SDK download into the workflow.

Linux:

```bash
Engine/BuildTools/prepare-workspace.sh linux-packages
```

Windows:

```powershell
py -3 Engine\BuildTools\buildtools.py prepare-host-workspace windows
```

Both routes end in `buildtools.py prepare-host-workspace <host> [features]`. The accepted feature names are listed in the [`prepare-host-workspace` reference](../../reference/buildtools/commands.md#prepare-host-workspace); omitting them selects the host defaults. System packages (`linux-packages`, `web-packages`, `android-packages`, `windows-cross-packages`) are explicit features and are installed through apt only on this host-provisioning step, so parallel jobs on one host do not contend for the package lock. Workspace features such as `web` or `android-arm64` do not install packages.

Practical rules:

- Add `--check` to verify availability without installing or building, for example as a preflight on a self-hosted runner.
- Select the workspace location with `FO_WORKSPACE`; by default it is `Workspace` under the current directory. Do not point `FO_EMSDK` at an unrelated SDK; BuildTools derives the pinned path from the workspace.
- `prepare-workspace.sh`, `prepare-mac-workspace.sh` and `prepare-win-workspace.ps1` forward every argument to `buildtools.py prepare-host-workspace`, which rejects a feature name that the host does not support with exit code 2 and lists the valid names, so a misspelled or unsupported feature fails the step instead of being ignored.
- The BuildTools command line is an `internal` contract that can change between revisions. Pin the Engine and read the reference generated for that revision.

macOS preparation is wrapped by `BuildTools/prepare-mac-workspace.sh`. macOS runners are unobserved for this guide; check the [support matrix](../../reference/platforms/support-matrix.md) before claiming a macOS lane.

## Use mirrors and workspace caches

`prepare-host-workspace` downloads toolsets, SDKs and LLVM sources from third-party hosts. A project that runs many jobs can put its own hosts in front of them. Nothing is compiled in; the addresses travel in the environment. Unset, each variable leaves the download path unchanged. The variables are documented in [Fetching through a mirror of your own](../build/index.md#fetching-through-a-mirror-of-your-own).

| Variable | Effect |
|---|---|
| `FO_DOWNLOAD_MIRROR` | Fetches `https://host/path` as `<mirror>/host/path` |
| `FO_WORKSPACE_CACHE` | Base URL for prepared workspaces (Emscripten, MSVC SDK tree), built once and downloaded whole |
| `FO_CI_TOKEN` | Bearer token sent only to the scheme and host of the two addresses above, never to an upstream host |
| `FO_CI_CA` | Path to a PEM file of extra trust anchors, added to the system store |

Behavior to rely on: a download is checked against the upstream `Content-Length`; an empty, unreachable or refusing workspace cache is a miss, not a failure; a corrupt Emscripten cache object is discarded and rebuilt. `FO_CI_CA` is used only when the path names an existing file.

Treat `FO_CI_TOKEN` as a secret (see below). Never place keystores, credentials, signed-session state or private config in a cache that other jobs restore.

## Write portable steps

CI steps run under different shells on different runners. Keep the logic out of the shell.

- Prefer one Python command per step. A step that is a single `python <script> <arguments>` call behaves the same on Windows, Linux and a developer machine, and its exit code is the step result.
- Resolve the interpreter once, in a setup step, and reuse the result. The Engine wrappers show the order: `py -3` or `python` on Windows, `python3` then `python` elsewhere. A bare `python` can resolve to Python 2 on a host, and BuildTools requires Python 3.
- Avoid shell features that differ between runners: `&&` chains in `cmd`, here-documents in PowerShell, `export`, glob expansion and path separators. Select the shell explicitly (`shell: bash` on every job that uses bash syntax, as the Engine workflow does through `defaults.run.shell`).
- Pass paths and values as arguments, not through inline interpolation of untrusted text.
- Let a script own multi-step logic and write one machine-readable result. A script is testable locally; a 40-line YAML `run:` block is not.
- Use UTF-8 for any file that carries non-ASCII text.

## Order and gate the jobs

Each stage answers one question and only runs when the previous answer is yes. Cheap checks come first.

```text
classify changes
  -> validate / format gates          (Engine checks through project wrappers)
  -> configure + native build
  -> compile scripts                  (CompileAngelScript and/or CompileManagedScripts)
  -> bake                             (BakeResources; ForceBakeResources for release)
  -> unit tests                       (RunUnitTests)
  -> gameplay / integration           (gameplay_test_runner.py)
  -> package                          (MakePackage-<package-id>)
  -> release                          (manual, protected, preflight first)
```

| Stage | Command shape | Gate |
|---|---|---|
| Format drift | `python Engine/BuildTools/buildtools.py format-content --check` ([Format Game Content Files](format-content.md)) | Fails on an unformatted main config, prototype or, with `--fos`, script file |
| Native audits | `python Engine/BuildTools/native_source_audit.py <audit> --config <file>` and `python Engine/BuildTools/native_safety_audit.py sync-scope` / `exception-safety` with `--scope`, `--config` and `--baseline` ([native source audits](native-source-audits.md), [native safety audits](native-safety-audits.md)) | Text audits need only Python; the clang-based audits run after configure and code generation in one Clang 20 lane with `--require-tooling` |
| Script checks | `python Engine/BuildTools/check_script_bindings.py` and `check_angelscript_hygiene.py` with the project root, scopes and baselines ([script checks](script-and-binding-checks.md)) | Fails on raw script-facing pointers, handler nullability drift, module hygiene findings and stale baseline rows |
| Content checks | `python Engine/BuildTools/check_text_packs.py <roots> --config <fomain>` ([text packs](content-and-diagnostics-tools.md#lint-text-packs-before-baking)) and `python Engine/BuildTools/migration_guard.py --config <file> --check` ([migration guard](migration-guard.md)) | Before the bake; never run `--update-manifest` in CI |
| Documentation links | `python Engine/BuildTools/markdown_links.py --root . --exclude Engine` or the project wrapper ([documentation checks](documentation-checks.md)) | Runs on documentation-only changes too |
| Project validation | Project-owned checks and generator `--check` modes | Fails fast on generated-file drift and authored-data errors |
| Compile scripts | `cmake --build <build-dir> --config <config> --target CompileManagedScripts` (or `CompileAngelScript`) | Build every scripting backend the project enables, on every role it ships |
| Bake | `... --target BakeResources`; `... --target ForceBakeResources` for release or when the input graph changed | Server and client metadata must come from one bake |
| Unit tests | `... --target RunUnitTests` | Exists when the project enables unit tests |
| Gameplay / integration | `python Engine/BuildTools/gameplay_test_runner.py --manifest <manifest> --report <report>` | Readiness, required and forbidden markers, one deadline, cleanup |
| Package | `... --target MakePackage-<package-id>` | Only after binaries and force-baked inputs exist |
| Release | Project-owned dispatch | Only from a protected revision |

The target names `CompileAngelScript`, `CompileManagedScripts`, `BakeResources` and `ForceBakeResources` are defined by the Engine's `ScriptsAndBaking` stage; `RunUnitTests` by the `Applications` stage; package targets are named by the project's own package declarations. Presets and build directories are project-owned, so the table shows shapes rather than a literal pipeline. See [Build, bake, then package](../release/packaging.md#build-bake-then-package) for the package sequence and [Gameplay and Integration Testing](../testing/gameplay-and-integration.md) for the runner's manifest and report contract.

Gating rules:

- Use job dependencies so a later stage never runs on a failed earlier one; use `fail-fast: false` for a platform matrix so one platform's failure does not hide another's.
- Never overlap a bake with a consumer of its output on the same output directory.
- Give each job a timeout. The example lanes use 90 minutes for the pinned matrix and 60 for the compatibility lane.
- Cancel superseded pull-request runs with a concurrency group, but never cancel a release or a main-branch run in progress. The Engine workflow cancels only for `pull_request` events.
- Set top-level `permissions: contents: read` and widen only per job.
- A partial pass count, a started background job or a skipped stage is not a pass. Require the terminal success marker and exit code.

## Skip runtime jobs for documentation-only changes

Runtime jobs (build, bake, tests, packages) are expensive. A cheap classification job computes the changed paths against the base revision and publishes booleans that later jobs use in their `if:` conditions.

- Define path groups once, in a script: documentation, content, scripts, native source, build files, CI files.
- Treat any path the script does not recognize as requiring the full lane. A new top-level directory must never silently skip a gate.
- Always run the full lane on the default branch, on schedule and on release dispatch. Classification only trims pull requests.
- Always run the validation jobs that apply to documentation (link and format checks, see [Check Documentation Links in a Game Project](documentation-checks.md)), even for a documentation-only change.
- Treat a change to the CI definition or the classifier itself as requiring every lane.

The Engine's own workflow does not classify paths; its `workflow_dispatch` `job` input selects a validation scope by hand instead. The classifier pattern is project-owned.

## Retain artifacts and evidence

Upload what a reviewer needs to reproduce or diagnose a result, not the build tree.

- Name artifacts by project, lane, platform, commit and run attempt, as the templates do: `<repository>-pinned-<os>-<sha>-<run_attempt>`. A re-run must not overwrite the first attempt.
- Set `if-no-files-found: error` so a lane that produced no evidence fails.
- Set explicit retention. The templates use 30 days for evidence; the Engine uses 14 days for documentation reports. Choose a period that covers the project's review and rollback window.
- Upload diagnostic evidence before a later, longer stage, and run the upload step with an always-run condition, so a late failure does not erase valid earlier evidence while the job still fails.
- Collect with a narrow, named list of patterns. The example collector retains reports, captures, package manifests and release archives and deliberately excludes recursive copies of `Build/**/Binaries`, which duplicate intermediate output and can exceed the job timeout. Never upload the whole workspace.
- Record the exact project commit, Engine commit, host, configuration and the primary-check result with each artifact. For packages also keep the SHA-256 manifest and payload inventory described in [Packaging and Release](../release/packaging.md).
- Keep runtime logs with the evidence. File names and rotation behavior are in [Runtime and Tool Log Files](../../reference/runtime/log-files.md).

## Protect secrets

The Engine's validation workflow is a reference for validation only; it does not sign or deploy, and its only secret use is a coverage upload token. A project release lane needs the controls in [CI trust boundaries](../release/security-and-secrets.md#ci-trust-boundaries). The CI-specific rules:

- Untrusted pull-request code never receives release, deployment, database or signing credentials. Run those jobs only from protected revisions in protected environments.
- Mask a secret before it enters the environment. With a runner that supports masking, register the mask first and only then export the value to later steps; a value exported before masking can already be in a log.
- Never echo, `set -x`, or print environment, command lines, generated config or provider responses in a secret-bearing step. Disable shell tracing explicitly.
- Pass a secret to the one step that needs it, as an environment variable scoped to that step, not as a command-line argument and not to the whole job.
- Masking is not a scanner: a transformed, split or short value escapes it. Prefer runtime `$TARGET_ENV` directives so secrets stay out of baked config, and scan package trees for forbidden files and synthetic canaries.
- Pin third-party actions and workflow revisions under project policy; treat a self-hosted runner as a persistent privileged host that is cleaned between jobs.
- Keep `FO_CI_TOKEN` scoped to the mirror it authenticates. It is a bearer credential with the same handling as any other.

## Dispatch releases manually

A release is a manual dispatch from a protected ref, not a side effect of a push. The pattern:

1. Inputs: the version or tag to publish and a dry-run flag.
2. Preflight job, no credentials: verify that the input version matches the project's version source, that the tag does not already exist, that the revision is on the protected branch, that the Engine gitlink matches the intended pin, and that the pinned lane is green for this commit. `release_preflight.py` performs the clean-tree, version, tag and Engine-gitlink reachability checks and treats every ref other than the release ref as a dry run; `project_version.py --check` validates the version file ([Release Tooling for a Game Project](release-tooling.md#preflight-a-release)). The pinned-lane result stays a workflow condition.
3. Build, bake with `ForceBakeResources`, test and package from the dispatched commit, using the same commands as the pinned lane.
4. Publish only when the dry-run flag is off and the ref is the protected release ref. On any other ref, run steps 1 to 3 and stop before publication, so a release workflow edit is testable on a branch without side effects.
5. Attach the manifest, hashes and evidence to the release, then run the post-publish verification in [Release Operations](../release/operations.md). `release_preflight.py --manifest` or `artifact_checksums.py` writes the SHA-256 manifest ([write package checksums](release-tooling.md#write-package-checksums)).

Do not let the release job fetch a floating Engine branch.

## State what the evidence qualifies

- A native build on one runner does not qualify another platform, architecture or ABI. Map each claim to the [support matrix](../../reference/platforms/support-matrix.md): compile-only, headless smoke and visible-client evidence are separate levels.
- Cross-builds and emulators are not device qualification. A headless test is not visible-client evidence.
- The Engine packaging fixtures prove package mechanics, not a game's signing, installer, store, deployment or rollback.
- Report an unobserved platform or owner-gated deployment as pending instead of inferring success from a neighboring lane.
- Keep targeted, full-suite, performance, platform and publication evidence as separate records.

## Validation checklist

1. The pinned lane runs on every pull request and protected-branch update, and the current-Engine lane runs on a schedule and on demand without changing the gitlink.
2. Checkout initializes only the named submodules, verifies the Engine gitlink, and orders LFS after endpoint configuration when LFS is used.
3. Runner preparation uses the Engine-owned command, with mirror and cache variables supplied as secrets or variables, not hardcoded.
4. Every step is a portable single command with an explicit shell and a once-resolved interpreter.
5. Jobs follow the order above, with timeouts, concurrency control and least-privilege permissions.
6. A change classifier trims documentation-only pull requests and fails open for unknown paths.
7. Artifacts carry commit-addressed names, explicit retention and a narrow collection list.
8. Secret-bearing jobs run only from protected revisions; a synthetic canary does not appear in logs, caches or artifacts.
9. Release dispatch has a preflight and a dry-run on non-release refs.
10. Every platform claim cites evidence from that platform.
