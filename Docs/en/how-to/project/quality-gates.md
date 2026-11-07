---
layout: default
title: Keep a Game Project Healthy: Formatting, Analysis, and Gates
locale: en
document_id: project-quality-gates
permalink: /Docs/en/how-to/project/quality-gates.html
---

# Keep a Game Project Healthy: Formatting, Analysis, and Gates

This guide separates what an embedding game project receives from the Engine without any work from what the project must add, and describes the discipline that keeps those checks meaningful: warnings are failures, exemptions carry written reasons, generated files are verified rather than hand-edited, and a gate is never lowered to get a green run. It ends with one verification order and a placement of each check in pre-commit, a local aggregate task and CI.

The Engine owns mechanisms and its own CI. It does not own a game's formatter layout, analyzer severities, baselines or acceptance thresholds, so this page names the project-side decisions explicitly.

## Source paths inspected

- `AGENTS.md`
- `BuildTools/buildtools.py`
- `BuildTools/package.py`
- `BuildTools/docs_support_matrix.py`
- `BuildTools/README.md`
- `BuildTools/format-source.sh`
- `BuildTools/format-source.cmd`
- `BuildTools/check_native_source_layout.py`
- `BuildTools/check_native_modules.py`
- `BuildTools/NativeModuleExceptions.json`
- `BuildTools/temporary_compat.py`
- `BuildTools/docs_metadata.py`
- `BuildTools/tests/test_docs_angelscript_style.py`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/RunTestExecutable.cmake`
- `Source/.clang-format`
- `Source/Common/Settings.inc`
- `Source/Tools/ManagedScriptBaker.cpp`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/Managed/Analyzers/FOnline.Analyzers.csproj`
- `Docs/SyncCoverAnalysis.md`
- `Docs/en/how-to/scripting/style-and-refactoring.md`
- `Docs/en/how-to/scripting/managed-csharp.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/contributing/coding-contracts/nullability.md`
- `Docs/en/contributing/coding-contracts/thread-safety-analysis.md`
- `Docs/en/contributing/coding-contracts/exception-safety.md`
- `Docs/en/contributing/coding-contracts/local-variables.md`
- `Docs/en/contributing/coding-contracts/smart-pointers.md`
- `Docs/en/contributing/testing/index.md`
- `Docs/en/reference/buildtools/commands.md`
- `Examples/NativeExtensionSample/CMakeLists.txt`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/generate_config.py`
- `.github/workflows/validate.yml`

## What the Engine supplies and what the project adds

| Area | Supplied by the Engine | Added by the project |
|---|---|---|
| Native formatting | `buildtools.py format-source` for Engine `Source/`; `FO_CLANG_FORMAT` binary override; clang-format major 20 gate | Its own `.clang-format`, the pinned binary, and a command that visits project directories |
| AngelScript formatting | Repair of `T?`, casts, `Item?[]` and named arguments after clang-format (`fix_fos_nullable_suffix`) | A wrapper that applies it to project `.fos` files |
| Managed C# compile checks | Generated project with `Nullable=enable`, `CheckForOverflowUnderflow`, `TreatWarningsAsErrors`, `EnforceCodeStyleInBuild` | `.editorconfig` severities, analyzer references and packages |
| Entity-cover analysis (Managed) | `FOnline.Analyzers.csproj` with `FOSYNC` diagnostics | Referencing it through `ManagedScript.Analyzers` |
| Native thread-safety analysis | `-Werror=thread-safety` on every Clang configuration, applied to first-party and project code | Annotating project locks; a Clang build lane |
| Native warnings | MSVC `/W4` without `/WX` | `-Werror` or `/WX` on project-owned targets |
| Script compile | `CompileAngelScript` and `CompileManagedScripts` targets | Running them for every role and failing on warnings |
| Local-variable, pointer-policy, exception-safety audits | Written rules only | Analyzer implementation, compile-database handling, baselines, CI policy |
| Generated-output checks | `--check` convention in Engine generators; `docs_metadata.py --check` | `--check` for every project generator |
| Layout and compatibility-date checks | `check_native_source_layout.py`, `temporary_compat.py` accept project paths | Choosing to run them |
| Sanitizers | `San_*` build types in `Init.cmake` | Presets that select them |

## Format source consistently

### Native C++ and C#

`python BuildTools/buildtools.py format-source` (wrappers: `BuildTools/format-source.sh`, `BuildTools/format-source.cmd`) formats only the Engine `Source/` tree, for the patterns `**/*.cpp`, `**/*.h` and `**/*.cs`. It does not discover a game's `SourceExt/` or `Scripts/`. The command reads no arguments (see the [generated CLI reference](../../reference/buildtools/commands.md)).

The binary is resolved in this order, and the result must report major version 20:

1. the path in `FO_CLANG_FORMAT`, when set;
2. `clang-format-20` on `PATH`;
3. `clang-format` on `PATH`.

Point `FO_CLANG_FORMAT` at the one binary the project pins, so every workstation and CI runner formats identically.

```bash
export FO_CLANG_FORMAT="$PWD/Tools/clang-format-20"
```

```powershell
$env:FO_CLANG_FORMAT = "$PWD\Tools\clang-format-20.exe"
```

The Engine ships `Source/.clang-format` with a `Cpp` section and a `CSharp` section, and Engine C# is formatted by clang-format as well. The Engine ships no style file for a game, so the project owns its `.clang-format` and must not rely on an ancestor directory outside its repository.

### AngelScript nullable syntax

Raw clang-format parses `.fos` as C++. It treats `?` as a binary operator and rewrites AngelScript nullable markers and named arguments:

```text
Before: Critter? target;     Item?[] items;     cast<Critter?>(x)     Create(count: 2)
Raw:    Critter ? target;    Item ? [] items;   cast < Critter ? > (x)   Create(count : 2)
```

Never run raw clang-format over `.fos` files. The repair is implemented in `BuildTools/buildtools.py` as `fix_fos_nullable_suffix`. It masks string literals, character literals and comments, restores the spacing in the forms above, and leaves literal text unchanged. `format_files(clang_format, root, patterns)` in the same module wraps clang-format, preserves BOM and line endings, leaves one final newline, applies the repair to every `.fos` path and returns the number of changed files.

`format-source` itself does not reach `.fos` files: its pattern list has no `*.fos`, and the Engine `Source/` tree holds none. A project therefore calls the functions directly. The Engine's own test (`BuildTools/tests/test_docs_angelscript_style.py`) calls `buildtools.format_files(..., ["**/*.fos"])` the same way, and other BuildTools scripts (`package.py`, `docs_support_matrix.py`) already import the module. These functions are module-level helpers, not a documented command-line interface, so pin the Engine revision, keep the wrapper small and cover it with a test that formats a nullable sample:

```python
import sys
from pathlib import Path

project_root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(project_root / 'Engine' / 'BuildTools'))

import buildtools

clang_format = buildtools.discover_clang_format()
changed = buildtools.format_files(clang_format, project_root / 'Scripts', ['**/*.fos'])
print(f'formatted {changed} file(s)')
```

The wrapper rewrites files in place. Its drift gate is the pair "run the formatter, then `git diff --exit-code`", which is also how Engine CI uses `format-source`. A project that reimplements the repair instead of importing it must keep it equivalent to `fix_fos_nullable_suffix`; the source comment in `buildtools.py` names exactly this duplication risk.

### Managed C# in a game

The Engine formats its own C# with clang-format, but nothing in `format-source` visits a game's `Scripts/*.cs`. Two other mechanisms apply to project C#:

- The generated script project always sets `EnforceCodeStyleInBuild=true`, so `IDE*` code-style rules run in the build at the severity an `.editorconfig` assigns. `ManagedScriptBaker` includes each `.editorconfig` found above a source file in its incremental stamp, so changing a severity recompiles the scripts.
- The Engine ships no `.editorconfig` for games. The project authors one and keeps it in its repository.

Whether a project also formats C# with clang-format through `buildtools.format_files` and its own `.clang-format` is a project choice; the Engine documents the mechanism only for its own sources.

## Treat warnings as failures

- Managed scripts: the generated project sets `TreatWarningsAsErrors=true`. Promoting an analyzer rule from `suggestion` to `warning` therefore makes it a build failure; roll rules out one severity step at a time. `ManagedScript.AnalysisLevel` and `ManagedScript.AnalysisMode` select the SDK analysis profile; when either is set the project also enables `EnableNETAnalyzers`.
- AngelScript: compile every role the change can reach (`SERVER`, `CLIENT`, `MAPPER`) through `CompileAngelScript`. The backend prints diagnostics as `<file>(<line>,<col>): warning : <text>`. The Engine sources inspected for this page contain no switch that converts these lines into a failing exit status, so a project that treats them as failures checks the output of the compile step for that pattern.
- Native: the Engine builds at MSVC `/W4` without `/WX` and treats a warning as a defect by policy, not by compiler flag. A project makes its own targets strict the way `Examples/NativeExtensionSample/CMakeLists.txt` does:

```cmake
if(MSVC)
    target_compile_options(NativeExtensionCore PRIVATE /W4 /WX)
else()
    target_compile_options(NativeExtensionCore PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
```

Apply such flags to project-owned libraries, not to bundled third-party targets. Clang builds additionally fail on thread-safety diagnostics for first-party and project code; MSVC and GCC never run that analysis, so keep at least one Clang lane. See [Thread Safety Analysis](../../contributing/coding-contracts/thread-safety-analysis.md).

## Pick the analysis a project can run

| Analysis | What the Engine provides | What a project must supply |
|---|---|---|
| Nullability | Compiler and runtime enforcement in both backends; Managed nullable diagnostics through the generated project | Event and remote-call declaration parity and other authoring rules are project-side analyzers; see [Nullability](../../contributing/coding-contracts/nullability.md) |
| Entity cover (Managed) | `FOSYNC001`-`FOSYNC005`, `FOSYNC009`-`FOSYNC011`, `FOSYNC015` in `FOnline.Analyzers.csproj`; `FOSYNC006`, `FOSYNC007` and `FOSYNC012`-`FOSYNC014` are retired | Add the project reference (below), set severities in `.editorconfig`, lower rules for test sources by a per-path `.editorconfig` section |
| Thread safety (native) | `FO_TSA_*` macros and the Clang flag | Annotations in project locks, an inventory of exemptions |
| Local variables | Rules plus the `FO_REDUNDANT_CONST_SUPPRESS` and `FO_USE_AFTER_MOVE_SUPPRESS` markers | The analyzer over `compile_commands.json`; `clang-query` and `clang-tidy` on the host; a pruned compile database, one command per translation unit |
| Pointer policy | Rules and wrapper tests | A project checker with exact line-level allowlists; the Engine ships none |
| Exception safety | Classification vocabulary and derivation rules | Any per-function baseline; the Engine ships none |
| Source layout | `check_native_source_layout.py --root <dir> --source-dir <dir>` | Running it on `SourceExt` |
| Temporary compatibility dates | `temporary_compat.py <dirs>` | Passing project directories |
| Sanitizers | `San_Address`, `San_Memory`, `San_Undefined`, `San_Thread` configurations | Presets and a Linux lane; the `validate.sh unit-tests-san-*` scenarios run the Engine's own validation project |

Wire the Managed analyzer through the main configuration (path relative to the project root, as in the [project configuration guide](../build/project-configuration.md)):

```ini
ManagedScript.Analyzers = Engine/Source/Scripting/Managed/Analyzers/FOnline.Analyzers.csproj
ManagedScript.AnalyzerPackages = <PackageId>,<exact-version>
ManagedScript.AdditionalFiles = <analyzer-config-file>
```

Package versions must be exact; entries containing `*`, `[`, `]`, `(`, `)`, `<`, `>`, `=` or `,` in the version are rejected, because a floating version makes the rule set depend on the day of the build. Editing an analyzer project, an additional file or an `.editorconfig` forces the scripts to recompile, so a new rule cannot stay silent behind a stale incremental result.

Two Engine checks accept a project tree directly. `check_native_source_layout.py` accepts a project root and a root-relative source directory; with `--root Examples/MinimalMultiplayer --source-dir SourceExt` it passes. It requires `Common.h` as the first include and exactly one final newline, so a deliberately standalone library file is reported. `check_native_modules.py` reads Engine CMake source lists and is not a project tool.

## Verify generated output instead of editing it

A generated file is never the repair target; fix its input or generator and regenerate. Give every generator a check mode that fails when the committed output is stale, and run it before bake or compile:

- Engine generators follow the pattern `--write` to produce and `--check` to verify. `BuildTools/docs_metadata.py --metadata <server> --metadata <client> --check` verifies a project's remote-call catalog against baked metadata (see [generated content](../build/generated-content.md)).
- `Examples/MinimalMultiplayer/generate_config.py --check` verifies a generated `.fomain`, and its `CMakeLists.txt` makes the check a dependency of `BakeResources` and `ForceBakeResources`. A project generator should follow this shape so a stale output stops the bake rather than reaching it.
- After regeneration, `git diff --exit-code` is the neutral drift gate for any output the tool writes in place.

## Keep baselines and exemptions honest

A baseline records known violations; an exemption file records deliberate exceptions. Both stay trustworthy under four rules:

1. Every row has a written, non-empty reason. The Engine's `BuildTools/NativeModuleExceptions.json` is a small model: each entry maps a path to a sentence, and `check_native_modules.py` rejects an empty reason. In-source suppressions follow the same idea; the local-variable markers require a reason of at least eight characters.
2. A stale row fails. When the code no longer needs the exception, the checker reports the row so it is deleted (`check_native_modules.py` fails with "is stale").
3. A baseline shrinks only. Exact line-level or identity-level entries are preferable to counts, because a count budget lets one new violation hide behind one fixed one. The smart-pointer guidance makes this point for allowlists.
4. A gate is never lowered, bypassed or baselined to turn a run green. Fix the failing owner, or add a reviewed exemption with its reason and an owner who will remove it. The Engine's own instructions state the same rule for builds and codegen.

## Order the aggregate route

Run checks in the order in which each one produces what the next consumes:

1. Generate: regenerate project-owned outputs, then reconfigure when CMake inputs changed.
2. Format: run the project formatters, then `git diff --exit-code` in check mode.
3. Analyze: static checks that do not need a bake (layout, analyzers, `temporary_compat.py`, project audits).
4. Validate: compile scripts for every role and run the bake plus content validators.
5. Test: native unit tests, then script and process-level tests (see [Add and Run Tests in a Game Project](project-tests.md)).

Generated-content ordering and the configure/compile/bake sequence are in [generated content](../build/generated-content.md). Do not run a bake and a consumer of its output concurrently.

## Place each check

| Check | Pre-commit | Local aggregate task | CI |
|---|---|---|---|
| Formatter in write mode | Yes, on staged files, restaging results | Yes | No; check mode only |
| Formatter drift check | No | Yes | Yes |
| Generator `--check` | Yes when fast | Yes | Yes |
| Layout, `temporary_compat.py` | Yes | Yes | Yes |
| Script compile, all roles | No | Yes | Yes |
| Bake and content validation | No | Yes | Yes |
| Native unit tests | No | Yes | Yes |
| Process-level gameplay tests | No | Optional | Yes |
| Sanitizers, long analyzers, package lanes | No | On demand | Yes, on a schedule or per change class |

The Engine ships no Git hook, so a pre-commit hook is a project file (for example one selected with `git config core.hooksPath`) that calls the same commands as the aggregate task. Keep hooks fast and never rely on them: CI repeats every check, because a hook can be skipped. Define the aggregate task in the workspace as described in [Set Up a VS Code Workspace for an FOnline Project](vscode-workspace.md), and mirror its steps in the pipeline described in [Set Up Continuous Integration for a Game Project](continuous-integration.md).

## Validation checklist

- `FO_CLANG_FORMAT` points at one pinned clang-format 20 binary on every workstation and runner.
- Project `.fos` files are formatted through the nullable-aware repair, never raw clang-format, and a test covers a `T?` sample.
- An `.editorconfig` and the analyzer settings are committed, and a severity change was followed by a recompile.
- Native project targets build with warnings as errors on MSVC and on one Clang lane.
- Every generator has a `--check` mode, wired before the consumer.
- Every baseline and exemption row has a reason; the checker fails on stale rows.
- The aggregate task runs generate, format, analyze, validate and test in that order, and CI repeats it.
