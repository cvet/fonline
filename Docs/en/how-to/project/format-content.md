---
layout: default
title: Format Game Content Files
locale: en
document_id: project-format-content
permalink: /Docs/en/how-to/project/format-content.html
---

# Format Game Content Files

`format-content` gives a game project one canonical layout for the formats the Engine owns: the main `.fomain` config, prototype files, and optionally AngelScript `.fos` scripts. It sorts settings and properties into a stable order, normalizes blank lines and `key = value` spacing, and re-reads every config and prototype rewrite with the Engine reader model so that a formatting run can never change what the Engine bakes. Run it before committing content edits and as a CI drift check; `format-source` remains the command for the Engine's own C++ and C# sources.

## Source paths inspected

- `BuildTools/format_content.py`
- `BuildTools/foconfig.py`
- `BuildTools/buildtools.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/tests/test_format_content.py`
- `Source/Common/ConfigFile.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/Settings.cpp`
- `Source/Common/FileSystem.cpp`
- `Source/Tools/Baker.cpp`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tools/ProtoBaker.cpp`
- `Source/Client/MapView.cpp`
- `Source/Tests/Test_ConfigFile.cpp`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`

## What it formats

The command takes its scope from the main config, the same way the baker does.

| Input | Where the files come from | Layout |
| --- | --- | --- |
| Main config | `--fomain`, otherwise the only `*.fomain` in the project root | Main-section settings first without a group (`Name`-style keys), then one block per group (`Group.Name`), groups sorted case-insensitively and separated by one blank line. Inside a group, settings follow the `SETTING` order of `Source/Common/Settings.inc`, then the project `///@ Setting` tags; unknown settings keep their authored order after known ones. `[SubConfig]` sections start with `Name` and `Parent` and use the same order without blank lines between groups. Every other section, such as `[ResourcePack]`, is kept as authored. |
| Prototype files | Every `[ResourcePack]` whose `Bakers` lists `Proto`: each `InputDirs` entry, filtered by `IncludePatterns`/`ExcludePatterns` with the Engine glob grammar, minus `Baking.IgnoreInputDirs`, keeping files whose extension is in `Baking.ProtoFileExtensions` (Engine default `fopro`) | Each `[Proto<Type>]` section becomes `$Name` and `$Parent`, then plain properties in metadata declaration order, then each component marker with its fields, then `$Text` entries, with one blank line between these groups. A section without `$Name` gets `$Name = <file name>`, the name the baker uses anyway. |
| AngelScript | Only with `--fos <glob>` relative to the project root | clang-format 20 plus the Engine AngelScript repair, exactly as the `buildtools.py` wrapper applies it. |

Property and setting order comes from the metadata files of every resource pack whose `Bakers` lists `Metadata`, sorted by their path inside the pack, which is the order `MetadataBaker` reads them. A complex type such as `dict<int32, string>` may span several tokens of a `///@ Property` tag. Properties declared natively by the Engine have no metadata-file position and keep their authored relative order after script-declared ones.

These sections are kept as authored apart from blank lines and trailing spaces:

- `[ProtoMap]` and nested `[<Name>/Item]`-style sections, because the Mapper writes map files itself (`MapView::SaveToText`) and a second layout would fight it;
- fixed-type sections such as `[QuestData]`;
- text before the first section.

A comment block directly above a setting or property moves with it. A comment block at the end of a section body describes the next section and moves above that header; a blank line between the block and the header is kept. A continued value (a line ending in a space and `\`) stays one entry with its physical lines unchanged.

Example input and output for a prototype with a component:

```ini
[ProtoItem]
$Text engl = "Rifle"
Weapon.DmgMax = 8
Weight = 5
# Shown in the barter screen
Cost = 10
Weapon = True
```

```ini
[ProtoItem]
$Name = Rifle

Weight = 5
# Shown in the barter screen
Cost = 10

Weapon = True
Weapon.DmgMax = 8

$Text engl = "Rifle"
```

## What it never changes

- Meaning. After formatting a config or prototype file, the tool parses the original and the result with `BuildTools/foconfig.py`, which mirrors `Source/Common/ConfigFile.cpp`: LF line splitting, ASCII whitespace trimming, continuation after a space or tab, `#` comments outside quotes and escapes, `+=` appends and last-assignment-wins. If any section name, key or value would differ, the file is left unchanged and the command fails. The only accepted difference is an added `$Name` equal to the file name.
- Encoding details. A UTF-8 BOM stays, LF or CRLF stays, and the file ends with exactly one line ending.
- Section order and the relative order of repeated keys, so `Key = a` followed by `Key += b` keeps its result.

The native reader does not remove a BOM: a BOM in front of a first `[ProtoItem]` line hides that header from the baker. `format-content` keeps such a file byte-for-byte and reports the BOM when a rewrite would change how the first line is read. Remove the BOM by hand in that case.

## Run it

From the root of a project that embeds the Engine at `Engine/`:

```powershell
py -3 Engine\BuildTools\buildtools.py format-content
py -3 Engine\BuildTools\buildtools.py format-content --check
```

```bash
python3 Engine/BuildTools/buildtools.py format-content
python3 Engine/BuildTools/buildtools.py format-content --check
```

`BuildTools/format_content.py` accepts the same options and can be run directly. Under `buildtools.py`, the project root is `FO_PROJECT_ROOT`, which BuildTools sets to the current directory when it is not set.

| Option | Effect |
| --- | --- |
| `--check` | Report files that need formatting, write nothing, exit with 1 when any file differs. |
| `--project-root DIR` | Root for relative paths and the `*.fomain` lookup. |
| `--fomain PATH` | Main config to read and format. |
| `--skip-fomain` | Read the main config for scope but do not format it. |
| `--skip-protos` | Do not format prototype files. |
| `--proto-path PATH` | Format only these files or directories instead of the `Proto` packs; repeatable. A file whose extension is not in `Baking.ProtoFileExtensions` is an error. |
| `--metadata PATH` | Take property and setting order from these files in this order instead of the `Metadata` packs; repeatable. |
| `--fos PATTERN` | Also format `.fos` files matching this glob; repeatable. |

For `.fos`, the clang-format binary is resolved as for `format-source`: `FO_CLANG_FORMAT`, then `clang-format-20`, then `clang-format` on `PATH`, and it must report major version 20. The Engine ships no binary. The style file is the project's own `.clang-format`, found by clang-format above each script. The full option list is in the [BuildTools command reference](../../reference/buildtools/commands.md) and the [helper CLI reference](../../reference/helper-cli/index.md).

## Exit codes

| Code | Meaning |
| --- | --- |
| `0` | Every file is formatted (after writing, or already in `--check` mode). |
| `1` | `--check` found at least one file that needs formatting. The files are listed as `Needs formatting: <path>`. |
| `2` | Error: no or several `*.fomain` files, a missing `--fomain`, an invalid `--proto-path`, a file that is not UTF-8, clang-format 20 not found, a rewrite that would change meaning, or a command-line usage error. Nothing is written for the failing file. |

## Wire it into a local task

Give the project one task that formats everything it owns, so authors do not need to remember the order. A minimal VS Code task, with the interpreter chosen per host as in [the workspace guide](vscode-workspace.md#choose-the-python-interpreter-per-os):

```json
{
  "label": "Format :: Content",
  "type": "process",
  "windows": {
    "command": "py",
    "args": ["-3", "Engine/BuildTools/buildtools.py", "format-content", "--fos", "Scripts/**/*.fos"]
  },
  "linux": {
    "command": "python3",
    "args": ["Engine/BuildTools/buildtools.py", "format-content", "--fos", "Scripts/**/*.fos"]
  },
  "problemMatcher": []
}
```

Drop the `--fos` arguments in a Managed C# project. Do not run the formatter while a bake that reads the same files is running.

## Add a CI drift check

Run the check after checkout and before any bake, so a formatting failure costs seconds:

```bash
python3 Engine/BuildTools/buildtools.py format-content --check
```

```powershell
py -3 Engine\BuildTools\buildtools.py format-content --check
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

`--check` lists the files that need formatting and does not write, so it is safe on a read-only checkout. Alternatively run the formatter and then `git diff --exit-code`, as Engine CI does for `format-source`; that variant also shows the expected diff in the job log. A `.fos` check needs clang-format 20 on the runner or `FO_CLANG_FORMAT` pointing at the pinned binary. Place the job with the other cheap static gates described in [Quality Gates](quality-gates.md) and [Continuous Integration](continuous-integration.md#order-and-gate-the-jobs).

## Extend it in a project

The Engine command owns only the Engine formats. A project keeps a thin wrapper that calls it with the project's roots and then runs whatever is project-specific:

```python
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[2]
CHECK = ['--check'] if '--check' in sys.argv[1:] else []

# Project-owned formatters, one command line each; every step must accept --check
PROJECT_STEPS: list[list[str]] = []


def main() -> int:
    engine = [sys.executable, str(PROJECT_ROOT / 'Engine' / 'BuildTools' / 'buildtools.py'), 'format-content', '--project-root', str(PROJECT_ROOT), *CHECK]
    codes = [subprocess.run([*engine, '--fos', 'Scripts/**/*.fos']).returncode]
    codes.extend(subprocess.run([*step, *CHECK]).returncode for step in PROJECT_STEPS)
    return max(codes)


if __name__ == '__main__':
    raise SystemExit(main())
```

Because the exit codes are ordered (`2` error, `1` drift, `0` clean), the wrapper's highest code is the right summary. Typical project steps are a GUI or dialog formatter, generated-source checks and a C# style pass.

- **Scope.** Narrow or widen the scope in the main config, where the baker reads it: `ExcludePatterns` and `Baking.IgnoreInputDirs` keep files out of both the bake and the formatter. For a temporary narrower run, pass `--proto-path`.
- **Exclusions with reasons.** The formatter has no exemption file. A file that must keep a hand layout belongs in a section the formatter keeps as authored (a fixed type or a map file), or outside the `Proto` packs. Write the reason as a `#` comment next to the `ExcludePatterns` entry; comments in the main config stay attached to the setting below them.
- **Project-specific checks.** Content audits, naming rules and generated-file checks run in the wrapper after `format-content`, each with its own exit code; keep them out of the Engine command.
- **Metadata outside the packs.** If a project keeps declarations that no `Metadata` pack reads, pass them with `--metadata` in the order the project wants; otherwise their properties sort as unknown.

### Move from an earlier project copy

Projects that kept their own `.fomain` and prototype formatters can switch in one commit:

1. Delete the project copies of the `.fomain` and prototype formatters and their tests; keep only project-specific steps.
2. Point the project format task and CI step at `buildtools.py format-content`.
3. Run the formatter once and commit the result as a pure formatting change. Expect a one-time diff where the earlier copy differed: settings and properties now follow the `MetadataBaker` file order, settings come from the current `SETTING` macros, all `[Proto<Type>]` sections except `[ProtoMap]` are laid out, a comment directly above a setting no longer gains a blank line, and the BOM is kept.
4. Run the bake and the project's content tests; the meaning check already guarantees identical parsed content, and the bake confirms the scope.

### How earlier copies were reconciled

Earlier per-project formatters shared one origin and diverged. The Engine command resolves every difference as follows:

| Behavior | Engine choice | Reason |
| --- | --- | --- |
| Default main config | `--fomain`, else the single `*.fomain` in the project root | Project names are parameters, not defaults. |
| Settings order source | `SETTING(...)` lines in `Settings.inc` | The current macro; older names and the older include file name are not read. |
| Metadata discovery | `Metadata` packs only, sorted like `MetadataBaker`; `--metadata` overrides | A build-tree `codegen-args.txt` depends on which tree was configured; the pack is what the baker reads. |
| Metadata extensions | `.fos` and `.cs` | `MetadataBaker` reads both. |
| Recursion into input dirs | The Engine glob: no patterns means every file, `*` stays in one directory, `**` crosses directories | Matches `FileSystem::FilterFiles`; a separate recursion flag is no longer a resource-pack key. |
| Comment above a setting | Stays attached, no blank line inserted | Blank lines separate groups only. |
| Trailing comment of a section | Moves to the next section header | It documents what follows. |
| Unknown component marker | Stays with its fields | Partial metadata must not split a component. |
| Prototype sections laid out | Every `[Proto<Type>]` except `[ProtoMap]` | The baker treats them alike; the Mapper owns map files. |
| BOM | Kept | The native reader does not strip it, so removing it changes what the Engine reads. |
| Continued lines, `+=`, quoted `#` | Read as the native reader does | Earlier copies could split or reorder them. |
| AngelScript nullable repair | One Engine implementation in `buildtools.py` | Covers template instances, funcdef types, return types followed by qualifiers or comments, and named arguments, and leaves ternaries alone. |

## Validation checklist

- `python3 Engine/BuildTools/buildtools.py format-content --check` exits with 0 on a clean checkout and 1 after an unformatted edit.
- A second formatting run changes nothing.
- The bake succeeds and content tests pass after the first formatting commit.
- CI runs the check before baking and fails on drift.
- Project-specific formatting runs in the project wrapper, not in the Engine command.
- For AngelScript, the runner has clang-format 20 and the project `.clang-format` is committed.
