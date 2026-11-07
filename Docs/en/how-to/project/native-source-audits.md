---
layout: default
title: Audit Native Source Rules in a Game Project
locale: en
document_id: project-native-source-audits
permalink: /Docs/en/how-to/project/native-source-audits.html
---

# Audit Native Source Rules in a Game Project

`BuildTools/native_source_audit.py` checks native C++ sources against the Engine coding contracts for allocation, smart pointers and local variables. The Engine runs it on its own `Source/` with an Engine-owned baseline; a game project runs the same command on its native extension directories with its own scopes and baseline, and keeps only a thin wrapper. This page shows the commands, the configuration and baseline formats, and how a project extends the audits without copying them.

## Source paths inspected

- `AGENTS.md`
- `BuildTools/native_source_audit.py`
- `BuildTools/native_source_audit/cli.py`
- `BuildTools/native_source_audit/common.py`
- `BuildTools/native_source_audit/allocator.py`
- `BuildTools/native_source_audit/smart_pointer.py`
- `BuildTools/native_source_audit/raw_pointer_ast.py`
- `BuildTools/native_source_audit/local_variables.py`
- `BuildTools/native_source_audit/explicit_local_types.py`
- `BuildTools/native_source_audit/clang_support.py`
- `BuildTools/native_source_audit/clang_audits.py`
- `BuildTools/native_source_audit/queries/redundant_local_const.query.in`
- `BuildTools/native_source_audit/queries/explicit_local_types.query.in`
- `BuildTools/audit_baselines/allocator.tsv`
- `BuildTools/audit_baselines/smart-pointer.tsv`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/tests/test_native_source_audit.py`
- `BuildTools/tests/test_native_source_audit_smart_pointer.py`
- `BuildTools/tests/test_native_source_audit_clang.py`
- `Source/Essentials/Containers.h`
- `Source/Essentials/MemorySystem.h`
- `Source/Essentials/SmartPointers.h`
- `Docs/en/contributing/coding-contracts/smart-pointers.md`
- `Docs/en/contributing/coding-contracts/local-variables.md`
- `Docs/en/contributing/coding-contracts/exception-safety.md`
- `Docs/en/contributing/coding-contracts/nullability.md`
- `Docs/en/reference/native/essentials.md`
- `Docs/en/how-to/project/quality-gates.md`

## What each audit enforces

| Audit | Engine rule it enforces | Needs |
|---|---|---|
| `allocator` | Engine code allocates through the memory-system vocabulary: no `std::` containers, no `std::format`/`std::to_string` and no bare `malloc`/`free` family outside reviewed boundaries ([out-of-memory contract](../../contributing/coding-contracts/exception-safety.md#1-out-of-memory-is-not-a-recoverable-error), [allocation vocabulary](../../reference/native/essentials.md#allocation-vocabulary), "Memory/callables/platform" in `AGENTS.md`) | Python only |
| `smart-pointer` | `ptr`/`nptr` borrows, `unique_*`/`refcount_*` owners, no `std::` owners, no reference members, nullable checks before dereference, raw pointers only at recognized ABI shapes ([pointer contracts](../../contributing/coding-contracts/smart-pointers.md#contracts), [raw pointer allowlist](../../contributing/coding-contracts/smart-pointers.md#raw-pointer-allowlist), [script binding boundary](../../contributing/coding-contracts/smart-pointers.md#script-binding-boundary-fo_script_api), [nullability](../../contributing/coding-contracts/nullability.md)) | Python only |
| `smart-pointer-ast` | The same raw-pointer rule from the AST: raw pointer variables, fields, parameters and returns on newly added lines | Clang 20+ `clang-query`, `compile_commands.json` |
| `local-variables` | [Redundant local `const`](../../contributing/coding-contracts/local-variables.md#redundant-local-const) and [use after move](../../contributing/coding-contracts/local-variables.md#use-after-move) | Clang 20+ `clang-query` and `clang-tidy`, `compile_commands.json` |
| `explicit-local-types` | [Explicit simple local types](../../contributing/coding-contracts/local-variables.md#explicit-simple-local-types) instead of `auto` | Clang 20+ `clang-query`, `compile_commands.json` |

Class layout, `[[nodiscard]]` placement, braced initialization and blank-line grouping are not checked by these audits; they remain review rules. The text audits check stable syntax markers and do not replace compilation.

Run the two text audits before every commit that touches native code and in every CI run. Run the clang-based audits where a compilation database exists, at least in one CI lane.

## Run the audits

From a standalone Engine checkout the default scope is `Source`. Windows (PowerShell):

```powershell
py -3 BuildTools\native_source_audit.py allocator
py -3 BuildTools\native_source_audit.py smart-pointer
```

Linux:

```bash
python3 BuildTools/native_source_audit.py allocator
python3 BuildTools/native_source_audit.py smart-pointer
```

From a game project root with the Engine at `Engine/`, the default scope is still the Engine `Source`; pass the project directories explicitly or through a configuration file:

```powershell
py -3 Engine\BuildTools\native_source_audit.py allocator --scope Engine/Source --scope SourceExt
py -3 Engine\BuildTools\native_source_audit.py smart-pointer --config Tools/NativeAudits/native-audits.json
```

```bash
python3 Engine/BuildTools/native_source_audit.py allocator --scope Engine/Source --scope SourceExt
python3 Engine/BuildTools/native_source_audit.py smart-pointer --config Tools/NativeAudits/native-audits.json
```

The clang-based audits need a directory with `compile_commands.json`, for example a CMake build configured with `CMAKE_EXPORT_COMPILE_COMMANDS=ON` and a Clang compiler, after code generation has produced the generated headers:

```bash
python3 Engine/BuildTools/native_source_audit.py local-variables --scope SourceExt --compilation-database Build/ClangAnalysis
python3 Engine/BuildTools/native_source_audit.py explicit-local-types --scope SourceExt --compilation-database Build/ClangAnalysis
python3 Engine/BuildTools/native_source_audit.py smart-pointer-ast --scope SourceExt --compilation-database Build/ClangAnalysis --diff-base origin/main
```

Without `clang-query`/`clang-tidy` (found through `--clang-query`, `--clang-tidy`, `FO_CLANG_QUERY`, `FO_CLANG_TIDY` or `PATH`) or without the compilation database, these audits print `skipped: <reason>` and exit 0. Add `--require-tooling` in the CI lane that provisions Clang so a missing tool fails instead. `smart-pointer-ast --diff-base` compares one git repository; run it from the repository that owns the scopes, because Engine files in a submodule are not part of the project's diff.

Useful options common to every audit:

| Option | Effect |
|---|---|
| `--root` | Directory that scopes, exclude patterns, reports, configuration and project baselines are relative to; default is the current directory |
| `--scope` | File or directory to audit; repeatable; replaces the configuration `scopes` |
| `--exclude` | fnmatch pattern on root-relative paths; `*` also crosses `/`; a pattern ending in `/` skips a subtree; repeatable and added to the configuration list |
| `--baseline` | Additional project baseline; repeatable |
| `--no-engine-baseline` | Shows every Engine finding, including reviewed ones |
| `--report` | Writes remaining findings and stale rows as TSV |
| `--emit-baseline` | Writes candidate baseline rows for the remaining findings, with an empty reason column |
| `--fail-on` | Text audits only: `error` (default), `warning` or `none` |
| `--mode` | Clang audits: `check` (default) fails on any active finding, `report` never fails, `apply` rewrites sources (`local-variables` needs `--checks redundant-local-const`) |

The exact option list is in the generated [helper CLI reference](../../reference/helper-cli/index.md).

## Exit codes

| Code | Meaning |
|---|---|
| 0 | No failing finding and no stale baseline row, or a clang-based audit skipped because tooling was missing |
| 1 | At least one finding at or above the failure threshold, or a stale baseline row |
| 2 | Invalid invocation, configuration or baseline, a scope outside `--root`, a missing scope, a Clang tool failure, or missing tooling with `--require-tooling` |

## Configure scopes and options

A configuration file keeps the project's choices out of command lines. All paths in it are relative to `--root`, and unknown keys fail:

```json
{
  "schema_version": 1,
  "scopes": ["Engine/Source", "SourceExt"],
  "exclude": ["SourceExt/vendor-*/"],
  "baselines": {
    "allocator": ["Tools/NativeAudits/allocator.tsv"],
    "smart-pointer": ["Tools/NativeAudits/smart-pointer.tsv"]
  },
  "compilation_database": "Build/ClangAnalysis",
  "smart_pointer": {
    "raw_return_guarded_files": ["SourceExt/JsonBridge.cpp"],
    "abi_line_patterns": ["\\bProjectSdkCallback\\b"],
    "nullable_factory_names": ["projectsdk_open_[A-Za-z0-9_]*"]
  }
}
```

- `scopes` and `exclude` select files; exclude vendored and third-party trees here rather than baselining them.
- `baselines` maps an audit name to project baseline files; every listed file must exist (a header-only file is valid).
- `compilation_database` is the directory with `compile_commands.json` for the clang-based audits.
- `smart_pointer.raw_return_guarded_files` adds project files in which a direct `return wrapper.get();` must go through a named `Return*`/`BorrowScript*` boundary, as the Engine script method files do.
- `smart_pointer.abi_line_patterns` adds regular expressions for project ABI shapes (callbacks, exported C entry points); matching raw-pointer lines are classified as ABI markers instead of ordinary raw pointers.
- `smart_pointer.nullable_factory_names` adds nullable C factory names of project libraries; adopting their result directly into a non-null `unique_del_ptr<T>` fails.

Rules that name Engine files (for example the allocator owners `Source/Essentials/Containers.h` and `MemorySystem.*`, or the reviewed `optional<refcount_ptr<T>>` load contracts) apply only to files under the Engine `Source` directory. Every other file is project code and follows the strict null-style rules and the split `FO_NAMESPACE` script implementation check.

## Write and review baselines

A baseline is a UTF-8 TSV file with a fixed header and four non-empty columns:

```text
path	rule	code	reason
SourceExt/JsonBridge.cpp	StdContainerType	return std::string {"name"};	nlohmann::json returns std::string; converted at this boundary only.
SourceExt/Json/	StdContainerType	*	Translation layer whose whole job is building nlohmann::json values.
```

- `path` is a file, or a directory ending in `/` that covers its subtree. Project baselines are relative to `--root`; the Engine baseline `BuildTools/audit_baselines/<audit>.tsv` is relative to the Engine root and is translated, so it also applies when the Engine is scanned as `Engine/Source`.
- `rule` is the rule name from the finding.
- `code` is the source line with whitespace collapsed, matched independently of the line number so unrelated edits do not invalidate the row; `*` accepts any line of that rule in the path.
- `reason` explains why the site is correct. A row without a reason is rejected.

A row inside the audited scopes that matches no finding is reported as `StaleBaselineEntry` and fails, so a fixed site removes its row in the same change. Rows outside the current scopes are not checked, which lets a project run only `SourceExt` without tripping on Engine rows.

To add a row, inspect the code behind the finding first, then generate a candidate and write the reason:

```bash
python3 Engine/BuildTools/native_source_audit.py allocator --config Tools/NativeAudits/native-audits.json --emit-baseline Workspace/allocator-candidates.tsv
```

Copy only the reviewed rows into the project baseline and fill in the reason column. Prefer exact `code` rows; use `*` and directory rows for a whole translation layer whose purpose is the boundary. Clang audits also accept the in-source markers `FO_REDUNDANT_CONST_SUPPRESS: <reason>` and `FO_USE_AFTER_MOVE_SUPPRESS: <reason>` described in [local variables](../../contributing/coding-contracts/local-variables.md#use-after-move).

## Wire it into a local task and CI

Call the same command, or the project wrapper below, from the local aggregate task and from CI, in the "analyze" stage described in [quality gates](quality-gates.md#order-the-aggregate-route). The text audits need no build, so they run early on any runner with Python 3.12+. Put the clang-based audits after configure and code generation in the lane that has Clang 20+, with `--require-tooling`. Keep `--report` output as a CI artifact when a run fails.

## Extend it in a project

Keep only project data and a thin wrapper in the project; do not copy the audit code.

1. Add `Tools/NativeAudits/native-audits.json` (the name is a project choice) with the scopes, exclude patterns, baselines and smart-pointer options shown above.
2. Add one baseline per audit next to it, each starting with the header line; every row carries its reason in the same line, so a reviewer sees both together.
3. Add a wrapper that calls the Engine tool for each audit and returns the worst exit code:

```python
"""Run the Engine native source audits with this project's scopes and baselines."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENGINE_TOOL = ROOT / 'Engine' / 'BuildTools' / 'native_source_audit.py'
CONFIG = 'Tools/NativeAudits/native-audits.json'
AUDITS = ('allocator', 'smart-pointer', 'local-variables', 'explicit-local-types')


def main() -> int:
    worst = 0

    for audit in AUDITS:
        command = [sys.executable, str(ENGINE_TOOL), audit, '--root', str(ROOT), '--config', CONFIG, *sys.argv[1:]]
        worst = max(worst, subprocess.run(command, check=False).returncode)

    return worst


if __name__ == '__main__':
    sys.exit(main())
```

4. Put project-specific rules (for example a ban on a game-only API) in a separate project checker next to the wrapper with its own tests. Do not add them to the Engine audit, and do not hide an Engine rule finding in a project checker.
5. Scanning `Engine/Source` from the project is optional: the Engine CI already runs the text audits with the Engine baseline at every Engine revision. Keep it when the project wants a local proof after an Engine update.

A project that kept its own earlier copy of these audits migrates in one change: move the rows about project files from its old exception or allowlist files into the new four-column baselines (one row per rule, `*` for former whole-file exemptions, the old classification folded into the reason), drop rows about Engine files because the Engine baseline owns them, move vendored-directory prefixes into `exclude`, replace the old commands in tasks and CI with the wrapper, delete the copied tools and their self-tests, and run the wrapper until it exits 0 with no stale rows.

## Validation checklist

- `allocator` and `smart-pointer` exit 0 on the project scopes with the project baselines, and on the Engine `Source` with the Engine baseline.
- Every baseline row has a reason, and no `StaleBaselineEntry` is reported.
- Vendored and third-party directories are excluded in the configuration, not baselined.
- The clang-based audits run in at least one CI lane with `--require-tooling`, after code generation, against a `compile_commands.json` with one real parse per translation unit.
- `smart-pointer-ast --diff-base` runs from the repository that owns the audited files.
- The wrapper returns a non-zero code when any audit fails, and the local task and CI call the same wrapper.
