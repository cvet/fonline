---
layout: default
title: Text Pack, Move Sync and Managed Project Tools
locale: en
document_id: project-content-and-diagnostics-tools
permalink: /Docs/en/how-to/project/content-and-diagnostics-tools.html
---

# Text Pack, Move Sync and Managed Project Tools

This guide covers three small Engine tools that a game project runs from its own tasks and CI: `check_text_packs.py` lints raw `.fotxt` text packs before baking, `move_sync_analysis.py` turns `Network.MoveSyncTrace` log lines into per-scenario movement synchronization reports, and `buildtools.py managed-project-path` prints where the Managed C# baker writes the generated project and solution. Each section gives the commands, exit codes and configuration, then shows how a project adds its own rules on top without copying the tool.

All three read only Engine-defined formats: the text-pack grammar, the `MOVESYNC` trace lines and the `ManagedScript` settings. Language choices, scenario harnesses, acceptance thresholds and IDE automation stay in the project.

## Source paths inspected

- `BuildTools/check_text_packs.py`
- `BuildTools/move_sync_analysis.py`
- `BuildTools/buildtools.py`
- `BuildTools/foconfig.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/tests/test_check_text_packs.py`
- `BuildTools/tests/test_move_sync_analysis.py`
- `BuildTools/tests/test_buildtools_managed_project.py`
- `BuildTools/tests/fixtures/move_sync`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/codegen.py`
- `Source/Common/TextPack.cpp`
- `Source/Tools/TextBaker.cpp`
- `Source/Common/Movement.h`
- `Source/Common/Movement.cpp`
- `Source/Common/Geometry.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/Settings.cpp`
- `Source/Essentials/Logging.cpp`
- `Source/Client/Client.cpp`
- `Source/Client/CritterHexView.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/Player.cpp`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Tools/ManagedScriptBaker.cpp`
- `Docs/en/how-to/content/text-and-localization.md`
- `Docs/en/explanation/authority-and-networking/index.md`
- `Docs/en/how-to/scripting/managed-csharp.md`
- `Docs/en/how-to/build/project-configuration.md`

## Choose the tool

| Tool | Reads | Run it when |
| --- | --- | --- |
| `Engine/BuildTools/check_text_packs.py` | Raw `.fotxt` files and `Baking.BakeLanguages` | Text or translations change; before every bake in CI |
| `Engine/BuildTools/move_sync_analysis.py` | Server and client logs with `MOVESYNC` lines | After a playtest or capture with `Network.MoveSyncTrace` enabled |
| `Engine/BuildTools/buildtools.py managed-project-path` | The main `.fomain` and an optional sub-config | Opening the generated C# solution, or a tool needs its path |

The commands below run from the project root of a repository that embeds the Engine at `Engine/`. On a standalone Engine checkout drop the `Engine/` prefix. Exact usage and help text are in the [helper command reference](../../reference/helper-cli/commands.md) and the [BuildTools command reference](../../reference/buildtools/commands.md). The Linux commands were exercised through a POSIX shell on Windows; macOS is unobserved.

## Lint text packs before baking

The lint parses every `<TextPack>.<Language>.fotxt` file under each root exactly as `TextPack::LoadFromString` does and simulates the language normalization of `TextPack::FixPacks` for the ordered `Baking.BakeLanguages` list (see [language normalization](../content/text-and-localization.md#language-normalization)). Each root is one resource-pack input; packs in different roots are not compared.

| Rule | Meaning | What baking would do |
| --- | --- | --- |
| `invalid-file-name` | The name is not `<TextPack>.<Language>.fotxt` | Fail |
| `invalid-utf8` | The file is not UTF-8 | Store undecodable bytes |
| `parse-error` | No second field, no closed third field, or an empty Key1 | Fail with `Invalid text file` |
| `unknown-language` | The language is not declared | Warn and skip the file |
| `duplicate-pack-file` | A second file for one pack and language in one root | Keep one, ignore the other |
| `duplicate-variant` | The same text twice under one complete key | Keep both; one is never told apart |
| `wrong-script` | Letters of a script that `--script` does not allow for the language | Ship the text |
| `orphan-pack` | A pack its fallback language does not have | Drop the pack |
| `extra-key` | A key its fallback language does not have | Drop the key |
| `missing-pack` | A pack the language lacks (silenced by `--sparse`) | Copy the fallback pack |
| `missing-key` | A key the language lacks (silenced by `--sparse`) | Copy the fallback text |
| `variant-count-mismatch` | Another variant count than the fallback (only with `--strict-variant-counts`) | Keep both counts |

A letter's script is the first word of its Unicode name in lower case (`latin`, `cyrillic`, `greek`, `arabic`, `hangul`, `katakana` and so on, `han` for CJK ideographs); digits, punctuation and spaces are never checked. Engine inline color tags are removed before the check. Project placeholders are project syntax, so remove them with `--ignore-pattern`. Use `--sparse` for an overlay language that intentionally inherits most text from its parent: it may omit packs and keys but still may not add keys its parent lacks.

Windows (PowerShell):

```powershell
py -3 Engine\BuildTools\check_text_packs.py Texts --config MyGame.fomain `
    --script engl=latin --script russ=cyrillic `
    --ignore-pattern '@[A-Za-z_]+@' `
    --baseline Tools\TextPacks\text-pack-baseline.json
```

Linux (bash):

```bash
python3 Engine/BuildTools/check_text_packs.py Texts --config MyGame.fomain \
    --script engl=latin --script russ=cyrillic \
    --ignore-pattern '@[A-Za-z_]+@' \
    --baseline Tools/TextPacks/text-pack-baseline.json
```

`--config` reads `Baking.BakeLanguages` from the main section and, with `--sub-config`, from that sub-config and its parents; `--languages 'engl russ'` passes the declarations directly instead. Each finding is one line, `path:line: rule: key detail`, where line `0` marks a finding that belongs to a file or a missing key rather than to a line. `--json <file>` also writes the remaining findings for a report or for drafting a first baseline.

| Exit code | Meaning |
| --- | --- |
| `0` | No finding is left after the exemptions |
| `1` | At least one finding or stale exemption remains |
| `2` | Usage error: unreadable config or baseline, invalid declarations, unknown language in `--script` or `--sparse`, invalid pattern, missing root |

### Baseline format

A baseline accepts known findings while a backlog is worked off. Every exemption names the rule, the path relative to `--base-dir` (the current directory by default), the complete key (`{Key1}{Key2}`, empty for file-level findings) and a reason. An exemption that matches nothing is reported as `stale-exemption`, so the file can only shrink. Several `--baseline` files may be passed.

```json
{
  "schema_version": 1,
  "exemptions": [
    {
      "rule": "missing-key",
      "path": "Texts/Game.russ.fotxt",
      "key": "{Intro}{}",
      "reason": "Intro text is translated together with the rest of the opening chapter."
    }
  ]
}
```

Unknown fields, unknown rules, empty reasons and duplicate exemptions are usage errors. The Engine has no `.fotxt` content and ships no baseline.

## Analyse movement synchronization traces

With `Network.MoveSyncTrace` enabled, the server and every client write one `MOVESYNC` line per movement synchronization event; the event and field list is in [Movement synchronization trace](../../explanation/authority-and-networking/index.md#movement-synchronization-trace). The analyzer merges several logs, cuts them into scenarios and compares where the server holds a critter, where its own client draws it and where each other client sees it.

### Record a trace

Enable the setting only for diagnostic runs, for example in a sub-config selected with `-ApplySubConfig MoveSyncTrace` (see [precedence](../build/project-configuration.md#understand-precedence)), or as the command-line override `-Network.MoveSyncTrace True`:

```ini
[SubConfig]
Name = MoveSyncTrace
Network.MoveSyncTrace = True
```

Scenarios are delimited by lines that the project writes itself from a script or test harness through its logging call, in the same format as the Engine lines:

```text
MOVESYNC side=cl ev=mark t=<monotonic microseconds> st=<synchronized milliseconds> viewer=<chosen critter id> label=begin:<name>
MOVESYNC side=cl ev=mark t=<monotonic microseconds> st=<synchronized milliseconds> viewer=<chosen critter id> label=end:<name>
```

The `viewer` of the begin mark is the acting critter. `t` must come from the same monotonic clock as the Engine lines; a script's frame time is that clock at the start of the frame, minus any pause or `Timer.DeltaTimeCap` offset. Labels cannot contain spaces. An optional `ev=input` line with `viewer`, `state` and `pattern_ms` records a scripted input change and feeds the input-to-send latency. Without marks, `--actor <critter id>` analyses the whole trace as one scenario.

### Run the analyzer

Windows (PowerShell):

```powershell
py -3 Engine\BuildTools\move_sync_analysis.py (Get-ChildItem Logs\MoveSync\*.log) `
    --geometry hexagonal --timeline blocked `
    --json Logs\MoveSync\report.json --markdown Logs\MoveSync\report.md
```

Linux (bash):

```bash
python3 Engine/BuildTools/move_sync_analysis.py Logs/MoveSync/*.log \
    --geometry hexagonal --timeline blocked \
    --json Logs/MoveSync/report.json --markdown Logs/MoveSync/report.md
```

`--geometry` is required and must match the project's `FO_GEOMETRY` (`hexagonal` or `square`), because every distance uses the Engine's grid metric. Logs from one machine share the default `--clock t`; for logs from several machines use `--clock st`, whose cross-process delays are estimates. `--settle-ms` extends each window after its end mark, `--timeline <label>` or `--timeline all` prints the merged event timeline, and `--require-converged` turns a disagreement at the end of any scenario into exit code `1`.

The Markdown table summarizes each scenario; the JSON report holds the details. The most useful keys per scenario are `converged` (server, actor and every observer still in view end on one hex), `settledAtMs`, `server.behindActorMs` and `observerViews.<id>.behindServerMs` (per-hex lag), `actor.snaps` and `correctionPx` (visible corrections), `divergence` (hex-seconds and seconds apart), `frameStallMaxMs` and `server.stallMaxMs` (host starvation rather than protocol), `server.finishOutcomes` and `server.uplinkMs`.

| Exit code | Meaning |
| --- | --- |
| `0` | The report was written; with `--require-converged`, every scenario converged |
| `1` | `--require-converged` was given and at least one scenario diverged |
| `2` | A log is unreadable, no `MOVESYNC` line was found, no scenario was found, or an argument is invalid |

`BuildTools/tests/fixtures/move_sync` holds a server log and two client logs written in the exact field order of the Engine emitters; use them to try the command.

## Locate the generated managed project

`ManagedScriptBaker` writes `<ProjectName>.gen.csproj`, `<ProjectName>.gen.sln` and `FOnline.ManagedHost.gen.csproj` into `ManagedScript.GeneratedDir`. A relative directory is rooted at the main config's directory; an empty value selects `<build directory>/GeneratedSource/Managed` of the CMake build tree (see [generated project and assemblies](../scripting/managed-csharp.md#generated-project-and-assemblies)). The command reads the same settings from the main section and, with `--sub-config`, from that sub-config and its parents. `CompileManagedScripts` and the default `BakeResources` apply no sub-config, so pass `--sub-config` only for a baking target that selects one.

Windows (PowerShell):

```powershell
py -3 Engine\BuildTools\buildtools.py managed-project-path MyGame.fomain --build-dir Build\windows
py -3 Engine\BuildTools\buildtools.py managed-project-path MyGame.fomain --build-dir Build\windows --open
```

Linux (bash):

```bash
python3 Engine/BuildTools/buildtools.py managed-project-path MyGame.fomain --build-dir Build/linux
python3 Engine/BuildTools/buildtools.py managed-project-path MyGame.fomain --build-dir Build/linux --json
```

The default output has four lines, `generated-dir:`, `project:`, `solution:` and `host-project:`, each followed by an absolute path. `--json` prints the same paths plus `projectExists` and `solutionExists`. `--build-dir` is needed only when `ManagedScript.GeneratedDir` is empty. `--require-existing` fails while the baker has not run yet. `--open` is the only option that starts anything: it opens the existing solution through the desktop file association (`os.startfile` on Windows, `open` on macOS, `xdg-open` elsewhere). Values that use `+=` or a `$ENV{}`/`$FILE{}` substitution are rejected rather than guessed.

| Exit code | Meaning |
| --- | --- |
| `0` | The paths were printed; with `--require-existing` or `--open`, the project and solution exist and the opener started |
| `1` | `--require-existing` or `--open` found the project or solution missing, or the opener failed |
| `2` | The config is unreadable, the sub-config or a parent is unknown, `ManagedScript.ProjectName` is empty, `ManagedScript.GeneratedDir` is empty without `--build-dir`, a value is unsupported, or an argument is invalid |

## Wire the tools into tasks and CI

Add one task per tool beside the project's other tasks, with per-OS interpreters as in the [VS Code workspace](vscode-workspace.md) guide:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Check :: Text Packs",
      "type": "shell",
      "windows": { "command": "py", "args": ["-3", "Tools/TextPacks/check_texts.py"] },
      "linux": { "command": "python3", "args": ["Tools/TextPacks/check_texts.py"] },
      "problemMatcher": []
    },
    {
      "label": "Tools :: Open Managed Solution",
      "type": "shell",
      "windows": { "command": "py", "args": ["-3", "Engine/BuildTools/buildtools.py", "managed-project-path", "MyGame.fomain", "--build-dir", "Build/windows", "--open"] },
      "linux": { "command": "python3", "args": ["Engine/BuildTools/buildtools.py", "managed-project-path", "MyGame.fomain", "--build-dir", "Build/linux", "--open"] },
      "problemMatcher": []
    }
  ]
}
```

In CI, run the text-pack lint before the bake job in the fast lane, since it needs no build, and keep its JSON as an artifact. Run the movement analyzer in the job that produces the trace, after the processes have exited, with `--require-converged` only when the scenario harness owns convergence as a gate. Treat exit code `2` as a broken step, not as content findings. See [continuous integration](continuous-integration.md) for lanes and artifacts.

```bash
python3 Engine/BuildTools/check_text_packs.py Texts --config MyGame.fomain \
    --script engl=latin --baseline Tools/TextPacks/text-pack-baseline.json \
    --json "$ARTIFACTS_DIR/text-packs.json"
```

## Extend it in a project

Keep project policy in a thin, project-owned wrapper that calls the Engine tool with the project's scope and exemption file, then runs project-only rules beside it. Do not edit or fork the Engine scripts.

```python
"""Project text-pack gate: the Engine lint with this project's scope, then project-only rules."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENGINE_LINT = ROOT / "Engine" / "BuildTools" / "check_text_packs.py"


def check_project_rules(texts: Path) -> int:
    # Project-only rules, such as key naming or texts that must stay distinct, live beside the Engine call
    return 0


def main() -> int:
    engine = subprocess.run([
        sys.executable, str(ENGINE_LINT), "Texts",
        "--config", "MyGame.fomain",
        "--script", "engl=latin",
        "--script", "russ=cyrillic",
        "--ignore-pattern", "@[A-Za-z_]+@",
        "--baseline", "Tools/TextPacks/text-pack-baseline.json",
    ], cwd=ROOT, check=False)
    return engine.returncode or check_project_rules(ROOT / "Texts")


if __name__ == "__main__":
    raise SystemExit(main())
```

- **Text packs.** Put language-specific policy (variant parity for selected packs, keys that must not share a text, edition overlays) in `check_project_rules`, reading `.fotxt` with the project's own reader or the Engine module's `parse_text_pack`. Keep the baseline next to the wrapper, one reason per exemption, and remove an exemption in the same change that fixes it; the stale-exemption rule enforces this.
- **Movement traces.** Import the module and pass `extensions` to `analyze`: each extension receives the scenario window and its report and returns extra data under `extensions.<name>`. The window's events start two minutes before the begin mark, so positions known earlier carry into the scenario; filter by `window.scenario.begin` and `end` when a metric must cover the scenario alone. Lines of a project-written side, such as a network-impairment relay, are read with `--extra-side` or by passing the side to `parse_logs`. Pass/fail thresholds (minimum frame rate, maximum lag per network profile) belong in the wrapper.

```python
"""Project movement report: the Engine analysis plus a metric over the project's relay lines."""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Engine" / "BuildTools"))

import move_sync_analysis as msa  # noqa: E402


def relay_drops(window: msa.ScenarioWindow, report: dict) -> int:
    begin = window.scenario.begin
    end = window.scenario.end + window.settle_us
    return sum(event.int_field("drop") for event in window.events
               if event.side == "relay" and begin <= event.time(window.clock) <= end)


def main(logs: list[str]) -> int:
    events = msa.parse_logs([Path(log) for log in logs], (*msa.ENGINE_SIDES, "relay"))
    report = msa.analyze(events, geometry="hexagonal", extensions={"relayDrops": relay_drops})
    print(json.dumps(report["summary"], indent=2))
    return 0 if report["summary"]["converged"] == report["summary"]["scenarios"] else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
```

- **Managed project.** IDE-specific behavior, such as reusing a running IDE instance or jumping to a file and line, stays in a project script that reads `managed-project-path --json` and acts on `solution` or `project`.

To migrate from a project's own earlier copy:

1. Replace each invocation with the Engine path and add the now explicit inputs: `--geometry` for the analyzer, `--script` and `--sparse` for the lint, `--build-dir` for the locator.
2. Move project-only rules and metrics into the wrapper or an extension. The analyzer's JSON keys are unchanged, except that the scenario's critter id is `actorId` and project relay metrics are no longer built in.
3. Convert old exemption lists into the baseline format, writing a reason for each row, and run the gate until it passes with no stale exemption.
4. Delete the old copy and its tests once the wrapper runs in the local aggregate task and in CI.

## Validation checklist

- The text-pack gate runs from a clean checkout with only Python 3.12 and exits `0`; an injected duplicate variant or extra key makes it exit `1`.
- Every baseline exemption has a reason, and none is reported stale.
- `--script` covers every shipped language, and `--sparse` names only overlay languages.
- The movement analyzer is given the project's real geometry, and its report lists every scenario the harness marked.
- `managed-project-path --require-existing` exits `0` after `CompileManagedScripts`, and `--open` opens the solution the baker just wrote.
- No project script copies or edits files under `Engine/BuildTools`.
