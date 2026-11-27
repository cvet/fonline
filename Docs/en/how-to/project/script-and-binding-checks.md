---
layout: default
title: Check Script Bindings and AngelScript Hygiene
locale: en
document_id: project-script-and-binding-checks
permalink: /Docs/en/how-to/project/script-and-binding-checks.html
---

# Check Script Bindings and AngelScript Hygiene

Two read-only Engine checks catch script-boundary mistakes before a build or a bake does. `check_script_bindings.py` keeps script-facing native signatures on the `ptr<T>`/`nptr<T>` vocabulary and compares AngelScript handlers with the event and remote-call declarations they implement. `check_angelscript_hygiene.py` checks the module conventions the AngelScript backend relies on. Both accept a project root, project scopes and a count-ratchet baseline, so a game project runs the Engine tools instead of keeping its own copies.

## Source paths inspected

- `BuildTools/check_script_bindings.py`
- `BuildTools/check_angelscript_hygiene.py`
- `BuildTools/angelscript_source.py`
- `BuildTools/finding_baseline.py`
- `BuildTools/check_native_source_layout.py`
- `BuildTools/codegen.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/tests/test_check_script_bindings.py`
- `BuildTools/tests/test_check_angelscript_hygiene.py`
- `BuildTools/tests/test_finding_baseline.py`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/AngelScript/AngelScriptEntity.cpp`
- `Source/Scripting/AngelScript/AngelScriptHelpers.cpp`
- `Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp`
- `Source/Server/Server.h`
- `ThirdParty/AngelScript/sdk/angelscript/source/as_datatype.cpp`
- `Examples/MinimalMultiplayer/Scripts/Tutorial.fos`
- `Docs/en/contributing/coding-contracts/nullability.md`
- `Docs/en/how-to/scripting/style-and-refactoring.md`

## When to run them

| Check | What it proves | Run it |
| --- | --- | --- |
| `check_script_bindings.py` | No raw `T*` reaches a script-facing native signature; stored callbacks keep the exported signature; AngelScript handlers agree with declarations about `null` | After editing `SourceExt`, `///@ Event` or `///@ RemoteCall` declarations, or handler signatures; in CI on every change |
| `check_angelscript_hygiene.py` | Module namespace, include, side-macro, sort-directive and file-ending conventions | After adding, moving or splitting `.fos` files; in CI on every change |

Neither check replaces compilation. Codegen still rejects raw handle pointers during a build, and the AngelScript compiler still rejects `?` on value types and explicit `@` on implicit-handle types, so those two script rules are not repeated here. The [nullability contract](../../contributing/coding-contracts/nullability.md) and the [AngelScript style guide](../scripting/style-and-refactoring.md) own the rules; these tools only report violations early.

## Check script bindings

### Rules

| Rule | Reports |
| --- | --- |
| `raw-pointer` | A raw `T*` in a `///@ ExportMethod` or other `FO_SCRIPT_API` signature, a member listed by `///@ ExportRefType ... Export =`, an `FO_ENTITY_EVENT` payload, or `ScriptFunc`/`FindFunc`/`CheckFunc` template arguments. `///@ EngineHook` declarations, internal pointers and `#define` lines are outside the contract. |
| `obsolete-nullable-marker` | The removed `FO_NULLABLE` marker in code. |
| `stored-callback-signature` | A `ScriptFunc<...>` constructed from `std::move(<parameter>)` inside an `FO_SCRIPT_API` body that spells a different signature from that exported callback parameter, for example `nptr<Item>` exported but `ptr<Item>` stored. |
| `event-handler-nullability` | An AngelScript handler whose `?` markers differ from the event declaration, argument by argument. Declarations are native `///@ ExportEvent` events and `///@ Event` tags; handlers are functions passed to `<target>.<Event>.Subscribe(...)` and `[[Event]]` functions named after the event. An entity event's leading non-null owner parameter is accounted for. |
| `remote-call-handler-nullability` | A `[[ServerRemoteCall]]` or `[[ClientRemoteCall]]` function whose `?` markers differ from its `///@ RemoteCall` declaration; the implicit leading `Player` of a server handler is accounted for. |

The handler rules matter because the AngelScript compiler compares handle types but not the `?` marker when it matches a handler to an event funcdef or a remote-call declaration, so a nullable argument can reach a parameter the handler treats as non-null. A handler with the wrong argument count, or an event name declared twice with different nullability, is skipped because the compiler or the ambiguity makes the comparison meaningless. `--list-rules` prints the identifiers.

### Run it

From an Engine checkout, the defaults scan `Source`:

```powershell
py -3 BuildTools/check_script_bindings.py
py -3 BuildTools/check_script_bindings.py --scope Source --scope Examples --scripts Examples
```

```bash
python3 BuildTools/check_script_bindings.py
python3 BuildTools/check_script_bindings.py --scope Source --scope Examples --scripts Examples
```

From a game project that embeds the Engine at `Engine/`, pass the project root and include `Engine/Source` so native event declarations are known when handlers are compared:

```powershell
py -3 Engine/BuildTools/check_script_bindings.py --root . --scope Engine/Source --scope SourceExt --scripts Scripts --exclude 'SourceExt/ThirdParty/*'
```

```bash
python3 Engine/BuildTools/check_script_bindings.py --root . --scope Engine/Source --scope SourceExt --scripts Scripts --exclude 'SourceExt/ThirdParty/*'
```

A finding names the file, the line, the rule and the declaration it was compared with:

```text
SourceExt/ServerExtension.cpp:4: [raw-pointer] raw pointer 'Critter*' in an ExportMethod signature; use ptr<T> for a non-null or nptr<T> for a nullable script handle
Scripts/Main.fos:12: [event-handler-nullability] handler 'OnLogin' for event OnPlayerLogin disagrees with the declaration at Engine/Source/Server/Server.h:231: argument 2 'cr' is declared nullable but the handler takes non-null Critter
[ScriptBindings] 2 finding(s), 2 above the baseline, 0 stale baseline row(s)
```

Scopes and script directories are root-relative and must exist; `--exclude` takes `fnmatch` patterns over root-relative POSIX paths, where `*` also crosses directories. Native files are `.h`, `.hpp`, `.cpp`, `.cc` and `.cxx`; script files are `.fos`. The scan is lexical: comments and string literals are ignored, every `#if` branch is read, and Managed C# handlers are left to the generated nullable surface and Roslyn.

## Check AngelScript module hygiene

### Rules

| Rule | Default | Reports |
| --- | --- | --- |
| `namespace-matches-file` | on | The first top-level namespace differs from the file stem, or the module declares none. Options: `exceptions` (each with `path`, `namespace`, `reason`) and `require_namespace`. |
| `no-include` | on | An `#include` directive. The backend already includes every configured script file in one root module, so cross-module calls name the owner as `Namespace::Function()`. |
| `side-macro-value-test` | on | `#ifdef`, `#ifndef` or `defined()` applied to `SERVER`, `CLIENT` or `MAPPER`. Every side module defines all three as `0` or `1`, so only `#if SERVER` isolates a side. Option: `macros`. |
| `preprocessor-balance` | on | `#endif`, `#else` or `#elif` without an open block, and an unclosed block. |
| `sort-directive` | on | `Sort N` on a later line, which the backend ignores, and a first line whose `Sort ` is not followed by an integer, which the backend reads as `0`. |
| `file-ending` | on | Invalid UTF-8, a UTF-8 BOM, a missing final line ending, or extra empty lines at EOF. |
| `commented-out-code` | off | `//` lines that look like disabled statements or calls. |
| `file-size` | off | A module longer than `max_lines` lines (default 1500). |

The Engine documents no declaration order inside a module, so the tool checks only the file-level ordering the backend actually reads. Comment language, naming vocabulary and domain rules stay in the project.

### Run it

```powershell
py -3 BuildTools/check_angelscript_hygiene.py
py -3 Engine/BuildTools/check_angelscript_hygiene.py --root . --scripts Scripts --config Tools/QualityBaselines/angelscript_hygiene.json
```

```bash
python3 BuildTools/check_angelscript_hygiene.py
python3 Engine/BuildTools/check_angelscript_hygiene.py --root . --scripts Scripts --config Tools/QualityBaselines/angelscript_hygiene.json
```

Without `--scripts` the tool checks `Examples`, which is what an Engine checkout contains. `--list-rules` prints each rule with its default.

### Configuration

The configuration is a JSON object with `schema_version` `1`, an optional `exclude` list of `fnmatch` patterns, and `rules` keyed by rule identifier. Each rule accepts `enabled` and only its own options. Unknown keys, rules or options and an exception without a reason are errors, so a typo cannot silently disable a rule.

```json
{
  "schema_version": 1,
  "exclude": ["Scripts/Generated/*"],
  "rules": {
    "namespace-matches-file": {
      "exceptions": [
        {"path": "Scripts/ColorExt.fos", "namespace": "Color", "reason": "Extends the shared Color namespace instead of declaring its own."}
      ]
    },
    "no-include": {"enabled": false},
    "commented-out-code": {"enabled": true},
    "file-size": {"enabled": true, "max_lines": 1500}
  }
}
```

Exclude generated `.fos` output here and fix its generator instead; a hand-edited generated file is not a hygiene fix.

## Exit codes

| Code | Meaning |
| --- | --- |
| `0` | No finding above the baselines and no stale baseline row. `--write-baseline` and `--list-rules` also exit `0`. |
| `1` | At least one finding above its baseline, or a baseline row that allows more findings than remain. |
| `2` | Invalid input: a missing or escaping scope, an unreadable file, an invalid configuration or baseline, or an argument error. |

## Baselines and the ratchet

Adopting a check on existing code starts from a baseline instead of a lowered rule. A baseline is a UTF-8 tab-separated file with the header `rule`, `path`, `count`, `reason`; `#` lines are comments. A row accepts up to `count` findings of one rule in one exact root-relative file:

```text
rule	path	count	reason
raw-pointer	SourceExt/ServerExtension.cpp	1	The ping bridge predates ptr<T>; convert it with the bridge rewrite.
side-macro-value-test	Scripts/Main.fos	1	Legacy server block, converted with the module split.
```

1. Write a draft: `--write-baseline Tools/QualityBaselines/angelscript_hygiene.tsv` records the current counts with empty reasons.
2. Write a reason in every row. Loading rejects an empty reason, so an unexplained draft cannot become a gate.
3. Pass the file with `--baseline`. More findings than a row allows fail, and so does a row that allows more than remain, which makes every fix lower or delete its row: counts only go down.

`--baseline` is repeatable. Keep one file per owner, for example an Engine file and a project file beside it; a `rule` and `path` pair may appear in only one of them. A header-only file is valid and keeps the command line stable once every finding is fixed. The same format serves the native comment rules of `check_native_source_layout.py --comments`, described in the [source tree guide](../../contributing/source-tree/index.md#native-comment-check).

## Wire it into a local task and CI

Run both checks in the project's aggregate quality task, after formatting and before compiling, and in a CI job that needs only Python 3.12 and the checkout. A CI step follows this pattern:

```text
- name: Check script bindings and AngelScript hygiene
  run: python3 Tools/check_scripts.py
```

Keep the arguments in one project-owned script, as below, so the local task and CI cannot drift. See [Quality Gates](quality-gates.md) for the aggregate order and [Continuous Integration](continuous-integration.md) for runner setup.

## Extend it in a project

The Engine tools stay generic; a project adds its policy around them.

1. Add a thin wrapper that calls the Engine tools with the project's root, scopes, excludes, configuration and baselines:

```python
"""Project quality gate: run the Engine script checks with this project's scopes and baselines."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE_TOOLS = ROOT / "Engine" / "BuildTools"
QUALITY = ROOT / "Tools" / "QualityBaselines"

CHECKS = [
    [ENGINE_TOOLS / "check_script_bindings.py", "--root", ROOT,
     "--scope", "Engine/Source", "--scope", "SourceExt", "--scripts", "Scripts",
     "--exclude", "SourceExt/ThirdParty/*", "--baseline", QUALITY / "script_bindings.tsv"],
    [ENGINE_TOOLS / "check_angelscript_hygiene.py", "--root", ROOT, "--scripts", "Scripts",
     "--config", QUALITY / "angelscript_hygiene.json", "--baseline", QUALITY / "angelscript_hygiene.tsv"],
    [ENGINE_TOOLS / "check_native_source_layout.py", "--root", ROOT, "--source-dir", "SourceExt", "--shallow",
     "--comments", "--comment-baseline", QUALITY / "native_comments.tsv"],
]


def main() -> int:
    failed = False

    for command in CHECKS:
        result = subprocess.run([sys.executable, *(str(part) for part in command)], check=False)
        failed = failed or result.returncode != 0

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
```

2. Put project-only rules in a separate project script beside the wrapper and call it from the same task: comment language, naming vocabulary, retired gameplay identifiers, required callback signatures of project attributes, or helpers that duplicate an Engine API. Reuse `BuildTools/angelscript_source.py` (character classification, code masking, function and parameter parsing) and `BuildTools/finding_baseline.py` (the baseline format) instead of copying a lexer.
3. Keep every reason next to its exemption: in the baseline `reason` column or in the configuration's namespace `exceptions`. A reason names the owner and the condition that removes the row.
4. Disable a rule in the configuration only when the project's own style document says it does not adopt it, and record that decision in that document.

### Migrate from an earlier project copy

A project that kept its own nullability validator or script-quality tool moves onto the Engine tools in one change:

1. Map each old rule to an Engine rule. Native `ptr`/`nptr` signature checks become `raw-pointer` and `obsolete-nullable-marker`; event and remote-call parity becomes the two handler rules, which now also follow `Subscribe(...)` registrations; namespace, preprocessor balance, end-of-file, commented-code and file-size checks become the hygiene rules of the same meaning.
2. Keep the rules with no Engine counterpart in the project script from step 2 above. Delete the rules the compiler already enforces, such as `?` on value types and explicit `@` on implicit-handle types.
3. Delete the copied validators, their tests and their old baselines, and remove their CI steps and tasks.
4. Run the wrapper with `--write-baseline` for each check, write a reason into every row, then run it again without the option and confirm exit code `0`.
5. Expect new findings where the old copy matched handlers by name only: those are handlers registered through `Subscribe(...)`. Fix them or baseline them with a reason.

## Validation checklist

- Both checks exit `0` on the project with the committed configuration and baselines.
- Every baseline row and namespace exception has a reason that names its removal condition.
- No baseline row allows more findings than remain.
- Generated `.fos` files are excluded and their generators are fixed instead.
- The local aggregate task and CI call the same project wrapper.
- `Engine/Source` is in the binding scopes whenever `--scripts` is used, so native events are compared.
