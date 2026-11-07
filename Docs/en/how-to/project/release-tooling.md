---
layout: default
title: Release Tooling for a Game Project
locale: en
document_id: project-release-tooling
permalink: /Docs/en/how-to/project/release-tooling.html
---

# Release Tooling for a Game Project

The Engine ships four helper scripts for the release and update work of a game repository: `project_version.py` writes and checks a monotonic project `VERSION`, `release_preflight.py` validates a release before it is tagged, `artifact_checksums.py` writes SHA-256 checksums of finished packages, and `engine_upgrade_report.py` prepares the record of an Engine update. They are standard-library Python 3.12 scripts that read Git and never fetch, tag, push or publish. Project policy (numbering scheme, digit limits, tag prefix, release branch, asset names) stays in command options and one small JSON file that the project owns.

## Source paths inspected

- `BuildTools/project_version.py`
- `BuildTools/release_preflight.py`
- `BuildTools/artifact_checksums.py`
- `BuildTools/engine_upgrade_report.py`
- `BuildTools/engine_version.py`
- `BuildTools/package.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/tests/test_project_version.py`
- `BuildTools/tests/test_release_preflight.py`
- `BuildTools/tests/test_artifact_checksums.py`
- `BuildTools/tests/test_engine_upgrade_report.py`
- `Docs/en/how-to/project/versioning-and-releases.md`
- `Docs/en/how-to/project/continuous-integration.md`
- `Docs/en/how-to/migration/engine-upgrade.md`
- `Docs/en/how-to/release/packaging.md`

## Choose the tool

| Task | Command | Writes |
|---|---|---|
| Advance the project version before a commit, or check it in CI | `project_version.py` | `VERSION` (generation only) |
| Validate a release, split large assets, list their hashes | `release_preflight.py` | Split parts, a manifest and the files named by `--outputs-file` and `--summary-file`, when asked |
| Write or verify checksums of finished packages | `artifact_checksums.py` | `<name>.sha256` sidecars and an optional manifest |
| Prepare the record of an Engine pin update | `engine_upgrade_report.py` | A Markdown record on standard output or in `--output` |

Common rules:

- Run the scripts with Python 3.12 or newer: `py -3` on Windows, `python3` on Linux. On some hosts `python` is still Python 2.
- The commands on this page run from the project root with the Engine checked out at `Engine/`. From a standalone Engine checkout, drop the `Engine/` prefix.
- Exit status `0` is success, `1` is a failed check or a Git or file error (the message is on standard error), and `2` is a usage error from the argument parser.
- The exact options and help texts are in the generated [helper CLI reference](../../reference/helper-cli/commands.md).
- The tools that compare versions need the complete history. A shallow clone is rejected; in CI check out with full depth and tags.

## Generate and check the project version

`project_version.py` implements the generator that [Version and Release a Game Project](versioning-and-releases.md#give-the-game-a-generated-version) requires for a committed `VERSION` file.

Generation, the default mode:

1. If `VERSION` differs from `HEAD` (staged, unstaged or untracked), the file is validated and left as it is. This is also how the first seed value enters the repository.
2. Otherwise it reads `VERSION` at `--base-ref`, takes the highest value committed in the history of `HEAD` and of the base ref, and writes the scheme's next value after it, with an LF line ending.

`--check` validates the working-tree file and fails when it is malformed or lower than any value in the history of `HEAD` (and of `--base-ref`, when given). An equal value passes, so a commit that does not advance the version stays valid.

```bash
python3 Engine/BuildTools/project_version.py --base-ref origin/main
python3 Engine/BuildTools/project_version.py --check
```

```powershell
py -3 Engine/BuildTools/project_version.py --base-ref origin/main
py -3 Engine/BuildTools/project_version.py --check
```

Typical output:

```text
project_version: VERSION: 1.4.316 -> 1.4.317 (base origin/main: 1.4.316)
project_version: VERSION 1.4.317 is valid; reachable floor 1.4.316
```

The file holds one line with nothing around the value; one LF or CRLF line ending is accepted. A committed value that the scheme rejects fails both modes instead of being skipped. A missing or unreadable base ref is an error, not a silent skip.

### Built-in schemes and limits

| `--scheme` | Format | Next value |
|---|---|---|
| `patch-per-commit` (default) | `MAJOR.MINOR.PATCH`, non-negative integers without leading zeroes | `PATCH + 1` |
| `counter` | One non-negative integer | Value `+ 1` |

`--limit COMPONENT=N` gives a component an exclusive upper bound. A component that reaches its bound resets to zero and carries into the previous one; an exhausted first component is an error. Values outside a bound fail validation. The components are `major`, `minor` and `patch` for `patch-per-commit` and `value` for `counter`. With `--limit patch=10000 --limit minor=10`, `0.3.9999` advances to `0.4.0` and `0.9.9999` to `1.0.0`. No limit applies by default, so choose limits from every consumer of the value: the Windows `Wix` pack needs two to four numeric fields within the Windows Installer ranges, and a game that encodes the version into another number needs bounds that keep that encoding valid.

### Keep the settings in a file

`--config <file>` reads a JSON object with any of the keys `file`, `base_ref`, `scheme`, `limits` and `since_commit`; options given on the command line win. Unknown keys are rejected. `file` and a relative scheme file are resolved against `--root`, which defaults to the current directory.

```json
{
  "file": "VERSION",
  "base_ref": "origin/main",
  "scheme": "patch-per-commit",
  "limits": {"patch": 10000, "minor": 10}
}
```

When the project changed its encoding, `--since-commit <sha>` (or `since_commit`) ignores the values committed before that commit, so an old value of the previous encoding no longer sets the floor. The commit must be an ancestor of `HEAD` and of the base ref.

### Use a project-owned scheme

`--scheme <file.py>:<factory>` or `--scheme <package.module>:<factory>` loads a project scheme. The factory receives the `limits` mapping and returns an object with three methods: `parse(text)` returns a tuple of integers that orders like the versions and raises `ValueError` for an invalid text, `bump(value)` returns the next tuple, and `format(value)` returns the file text. A module name is imported with `--root` on the import path.

```python
class ReleaseCounterScheme:
    """r<number> versions that advance by a configurable step."""

    def __init__(self, limits):
        self.step = limits.get('step', 1)

    def parse(self, text):
        if not text.startswith('r') or not text[1:].isdigit():
            raise ValueError('expected r<number>')
        return (int(text[1:]),)

    def bump(self, value):
        return (value[0] + self.step,)

    def format(self, value):
        return f'r{value[0]}'


def create_scheme(limits):
    return ReleaseCounterScheme(limits)
```

Save it as `Tools/version_scheme.py` and pass `--scheme Tools/version_scheme.py:create_scheme`.

## Preflight a release

`release_preflight.py` replaces the inline shell preflight of a release job. In order, it:

1. rejects a shallow repository and tracked files that differ from `HEAD` (untracked files such as downloaded assets are allowed);
2. reads the version file and validates it with the same scheme options as `project_version.py` (`--config`, `--file`, `--scheme`, `--limit`);
3. derives the tag `--tag-prefix` (default `v`) plus the version and checks it with `git check-ref-format`;
4. collects problems: the tag exists locally (or on `--remote`), the version does not exceed the newest existing tag with that prefix, and, with `--engine Engine`, the committed Engine gitlink is not reachable from any remote-tracking branch of the submodule;
5. chooses the mode: `publish` only when `--ref` (default: the branch checked out at `HEAD`) equals `--release-ref` and `--dry-run` is absent; every other run is `dry-run`. A short ref such as `main` means `refs/heads/main`;
6. fails in `publish` mode when any problem was found, and only warns in `dry-run` mode;
7. with `--assets-dir`, validates the assets and optionally splits and hashes them.

```bash
python3 Engine/BuildTools/release_preflight.py --ref "$GITHUB_REF" --release-ref refs/heads/main \
  --remote origin --engine Engine \
  --assets-dir dist --max-asset-bytes 1900M --manifest SHA256SUMS
```

```powershell
py -3 Engine/BuildTools/release_preflight.py --ref refs/heads/main --release-ref refs/heads/main `
  --remote origin --engine Engine `
  --assets-dir dist --max-asset-bytes 1900M --manifest SHA256SUMS
```

Standard output is a stable `key=value` list; this one comes from a run with `--max-asset-bytes 2K`:

```text
mode=publish
version=1.4.317
tag=v1.4.317
previous_tag=v1.4.316
asset=948fe603f61dc036b5c596dc09fe3ce3f3d30dc90f024c85f3c82db2ccab679d  6  Game-Client.zip
asset=e5a00aa9991ac8a5ee3109844d84a55583bd20572ad3ffcd42792f3c36b183ad  2048  Game-Server.zip.part00
```

Assets and their outputs:

- The assets are the regular files at the root of `--assets-dir` that match a repeatable `--asset-pattern` glob (default: every file); `.sha256` sidecars and the manifest itself are excluded. No asset, an empty asset, a link or an unsafe file name is an error.
- `--max-asset-bytes SIZE` (with an optional `K`, `M` or `G` suffix, powers of 1024) splits every larger asset into `<name>.part00`, `<name>.part01` and so on, at most 100 parts. The parts are written to temporary files, their concatenation is compared with the digest of the original, and only then are they renamed into place and the original removed. An existing part of the same name stops the run. Hosts such as GitHub Releases limit the size of one asset; choose the limit below that cap.
- `--manifest SHA256SUMS` writes one manifest of the final assets (parts included) in the `sha256sum` format.
- `--outputs-file "$GITHUB_OUTPUT"` appends `version`, `tag`, `previous_tag`, `publish` (`true` or `false`) and `assets` lines; `--summary-file "$GITHUB_STEP_SUMMARY"` appends a Markdown table. Any CI system that reads `key=value` files can use the first.

Players reassemble split parts before extracting:

```bash
cat Game-Server.zip.part* > Game-Server.zip
sha256sum --check SHA256SUMS
```

```powershell
cmd /c copy /b Game-Server.zip.part00 + Game-Server.zip.part01 + Game-Server.zip.part02 Game-Server.zip
```

The preflight does not create the tag. Tag the exact commit after artifact acceptance, as the last step before publication, as described in [Tag the release](versioning-and-releases.md#tag-the-release).

## Write package checksums

`artifact_checksums.py` hashes finished packages after the last package writer has stopped and before the files are copied to their publication place.

```bash
python3 Engine/BuildTools/artifact_checksums.py Workspace/output/Game-Release --manifest SHA256SUMS
python3 Engine/BuildTools/artifact_checksums.py Workspace/output/Game-Release --manifest SHA256SUMS --verify
```

```powershell
py -3 Engine/BuildTools/artifact_checksums.py Workspace/output/Game-Release --manifest SHA256SUMS
py -3 Engine/BuildTools/artifact_checksums.py Workspace/output/Game-Release --manifest SHA256SUMS --verify
```

- The default selection is every root-level file ending in `.zip`, `.tar`, `.tar.gz`, `.msi` or `.apk`, compared case-insensitively. Repeat `--suffix .exe` (and so on) to replace that set, or repeat `--artifact <relative path>` to select nested files explicitly. Unpacked `Raw` package trees are not selected by the default scan.
- Each sidecar `<name>.sha256` holds one UTF-8 line, `<lowercase hex>  <leaf name>`, so `sha256sum --check <name>.sha256` works from its directory. The manifest lists every selected file by its path relative to the directory, sorted. `--no-sidecars` writes only the manifest.
- Every input is hashed before any output is staged; outputs go to temporary files that are renamed into place. A file that changes during or after hashing, an absolute or parent path, a link, CR, LF or a Windows-reserved character in a name is an error, and a failure leaves the previous checksum files in place.
- `--verify` recomputes the hashes and compares them with the existing sidecars and the manifest, which also works on Windows hosts without `sha256sum`. `(Get-FileHash -Algorithm SHA256 <file>).Hash.ToLower()` prints the same hex digest.

`BuildTools/package.py` does not write these files. One package directory is assembled by several invocations (a `SingleZip` archive is appended to by each part, and `package.py include` adds files afterwards), and signing can follow packaging, so only the release job knows when the bytes are final. [Packaging and Release](../release/packaging.md#reproducibility-and-provenance) asks for hashes of the files that will actually be published.

## Prepare an Engine update record

`engine_upgrade_report.py` runs from the project root and writes the Markdown record that [Upgrade an Embedding Project](../migration/engine-upgrade.md) describes, with the facts filled in and every judgement left as `<fill in>`.

```bash
python3 Engine/BuildTools/engine_upgrade_report.py --names update-names.txt --output Workspace/engine-update/record.md
```

```powershell
py -3 Engine/BuildTools/engine_upgrade_report.py --names update-names.txt --output Workspace/engine-update/record.md
```

- `--old` defaults to the parent gitlink at `HEAD`, and `--new` to the Engine checkout `HEAD`; pass a commit or a `v<version>` tag to compare other revisions without checking them out.
- The record contains the three Engine identities and `git submodule status`, the ancestry check, every commit of the range oldest first with empty owner, consequence and action columns, the first-parent publication steps that change `VERSION`, the changed `Docs/generated` models, each incoming changelog entry with its `### Migration` section quoted oldest first, the target's `## Unreleased` text, the single-token code spans quoted in those migrations as candidate names, the name search, and the verification and completion sections.
- `--names <file>` takes one literal name per line and runs `git grep -nIF` over the project outside the Engine checkout (restrict it with repeatable `--search-path`). Each name gets its match count and up to `--max-locations` locations; a name without matches is marked for its negative proof.
- The changelog is read at the target revision with `git show`, so the checkout may stay where it is. A baseline version without an entry in that changelog is reported in the record and as a warning, because the complete commit range must then be reviewed instead.
- A target that does not descend from the baseline still produces the record, marked `FAILED`, and exits with status `1`.
- An existing `--output` file is not replaced without `--force`, so a filled-in record is never overwritten by accident. The command needs an initialized Engine checkout with its own `.git`.

## Wire the tools into tasks and CI

A local task runs the same command the CI job runs. A VS Code task that advances the version, as one entry of the `tasks` array in `.vscode/tasks.json`:

```json
{
  "label": "Release :: Advance VERSION",
  "type": "process",
  "command": "py",
  "args": ["-3", "Engine/BuildTools/project_version.py", "--config", "Tools/version.json"],
  "options": {"cwd": "${workspaceFolder}"},
  "problemMatcher": []
}
```

On Linux hosts use `python3` as the command and drop `-3`. A release workflow then calls the tools in this order; the YAML below is a pattern for GitHub Actions, not a complete file:

```text
release:
  if: github.event_name == 'workflow_dispatch'
  steps:
    - uses: actions/checkout@v6
      with: {fetch-depth: 0, fetch-tags: true, submodules: true}
    - uses: actions/download-artifact@v7
      with: {name: Game-Release, path: dist}
    - run: python3 Engine/BuildTools/project_version.py --config Tools/version.json --check
    - id: preflight
      run: >-
        python3 Engine/BuildTools/release_preflight.py --config Tools/version.json
        --ref "$GITHUB_REF" --release-ref refs/heads/main --remote origin --engine Engine
        --assets-dir dist --max-asset-bytes 1900M --manifest SHA256SUMS
        --outputs-file "$GITHUB_OUTPUT" --summary-file "$GITHUB_STEP_SUMMARY"
    - if: steps.preflight.outputs.publish == 'true'
      run: <create the tag and the release from steps.preflight.outputs.tag>
```

Run `artifact_checksums.py` at the end of the package job when packages are published somewhere other than a release with a manifest, and `project_version.py --check` in the validation job of every change. Keep the publish step behind the `publish` output, and see [Dispatch releases manually](continuous-integration.md#dispatch-releases-manually) for the job boundaries and credentials.

## Extend it in a project

The Engine tools hold the reusable mechanics; the project keeps its policy next to them.

- **Thin wrapper.** Put the project's settings in `Tools/version.json` and call the Engine tool from a project script, so tasks, hooks and CI share one entry point:

  ```python
  import subprocess
  import sys
  from pathlib import Path

  ROOT = Path(__file__).resolve().parents[1]

  # Limits keep the value inside the save-format encoding; see the project versioning page
  command = [sys.executable, str(ROOT / 'Engine/BuildTools/project_version.py'),
             '--root', str(ROOT), '--config', str(ROOT / 'Tools/version.json'), *sys.argv[1:]]
  raise SystemExit(subprocess.run(command).returncode)
  ```

- **Project checks beside it.** Run game-specific checks after the Engine tool in the same task: for example that the version still converts into the game's data version, that a store version code moved with it, or that release notes exist for the new tag. Fail the task when either step fails.
- **Reasons next to settings.** The JSON config is strict and has no comment field. Record why each limit, `since_commit` or tag prefix exists in a comment beside the wrapper call or in the project's versioning page, and remove a setting together with its reason when it stops applying.
- **Own scheme.** When the built-in schemes do not express the policy, ship a scheme module in the project and pass it with `--scheme`, as shown above. Keep its tests in the project.
- **Migrating from a copied script.** A project that kept its own version generator, preflight shell step, checksum script or changelog-listing snippet deletes the copy and its tests, adds the config file and wrapper, and runs `project_version.py --check` once on the current history. Differences to check while migrating: a missing base ref and an invalid committed value now fail instead of being skipped (use `--since-commit` for an old encoding), an untracked `VERSION` counts as modified, leading zeroes are rejected, the default checksum suffixes do not include `.exe` (pass `--suffix`), and a split now happens for every asset larger than `--max-asset-bytes` with parts of at most that size.

## Validation checklist

1. `project_version.py --check` passes in CI on a full-depth checkout, and generation runs before the commits that need a new version.
2. Limits and `since_commit` are set from the consumers of the version, with their reasons recorded.
3. The release job runs `release_preflight.py` with `--release-ref`, so every other ref is a dry run, and publishes only when `publish=true`.
4. Split assets reassemble to the original digest, and the published manifest verifies with `sha256sum --check` or `artifact_checksums.py --verify`.
5. Checksums are written after the last package writer and signing step, from the files that are published.
6. Every Engine update starts with an `engine_upgrade_report.py` record saved outside the Engine checkout and completed before the update is called done.
