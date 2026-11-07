---
layout: default
title: Audit Entity Cover and Exception Safety in a Game Project
locale: en
document_id: project-native-safety-audits
permalink: /Docs/en/how-to/project/native-safety-audits.html
---

# Audit Entity Cover and Exception Safety in a Game Project

`BuildTools/native_safety_audit.py` checks native C++ sources against two Engine contracts: native code reached from a server script export must not obtain, replace or release entity cover, and every classified function keeps a reviewed exception-safety level with a written reason. The Engine audits its own `Source/` with these commands in CI; a game project runs the same commands with its native extension directories added, so project code is held to the same rules. This page gives the commands, the configuration and baseline formats, the CI pattern and the way a project extends the audits.

## Source paths inspected

- `AGENTS.md`
- `BuildTools/native_safety_audit.py`
- `BuildTools/native_safety_audit/scope.py`
- `BuildTools/native_safety_audit/sync_scope.py`
- `BuildTools/native_safety_audit/exception_safety.py`
- `BuildTools/native_safety_audit/cpp_parser.py`
- `BuildTools/audit_baselines/sync-scope.tsv`
- `BuildTools/audit_baselines/exception-safety.tsv`
- `BuildTools/tests/test_native_safety_audit_sync_scope.py`
- `BuildTools/tests/test_native_safety_audit_exception_safety.py`
- `BuildTools/HelperCliInterface.json`
- `Source/Server/EntitySync.cpp`
- `Source/Scripting/ServerGlobalScriptMethods.cpp`
- `Docs/en/how-to/scripting/lifecycle-and-concurrency.md`
- `Docs/en/how-to/scripting/managed-csharp.md`
- `Docs/en/contributing/coding-contracts/exception-safety.md`
- `Examples/MinimalMultiplayer/SourceExt/ServerExtension.cpp`
- `.github/workflows/validate.yml`

## What each audit enforces

| Command | Engine rule it enforces | Owning contract | Fails on |
|---|---|---|---|
| `sync-scope` | Ordinary server script calls supply complete entity cover; native callees may call `ValidateEntityAccess` and `EnsureEntitySynced` but must not repair missing cover with `SyncEntity`/`SyncEntities`, `ScopedSyncContext::Sync`, direct `EntityLock` use, cover probing, or blocking, sleeping and retrying while obtaining cover. The cover-retention terminals themselves stay non-blocking. | `AGENTS.md` (Invariants, synchronization and lifetime), [native retention of covered entities](../scripting/lifecycle-and-concurrency.md#native-retention-of-covered-entities), [server entity synchronization](../scripting/managed-csharp.md#server-entity-synchronization) | A script export or registered `FO_SCRIPT_GENERIC` callback whose call graph reaches a forbidden sink |
| `exception-safety` | Each function's exception-safety level (`NoThrow`, `Strong`, `Basic`, `None`) is derived from its body and re-derived when the body changes. | [Exception safety, section 8](../../contributing/coding-contracts/exception-safety.md#8-per-function-exception-safety-classification-es-levels) | A new, changed or removed function without a matching row, a row without a reason, an unclassified row, a malformed row |

Both audits are pure Python 3.12 standard-library programs. They read source text only: no compiler, `clang`, `compile_commands.json`, bake or build is needed, and they run on any host with Python. The parser is lexical. It finds functions by brace shape and by the `FO_TRACE_ZONE` and `FO_SCRIPT_API` conventions, does not evaluate the preprocessor, and resolves calls conservatively by name and receiver type. A new function without a zone or an export prefix is not discovered by `exception-safety` until a baseline row names it, so review still covers such additions.

## Run the audits on the Engine

From an Engine checkout, on Windows:

```powershell
py -3 BuildTools/native_safety_audit.py sync-scope
py -3 BuildTools/native_safety_audit.py exception-safety
```

On Linux:

```bash
python3 BuildTools/native_safety_audit.py sync-scope
python3 BuildTools/native_safety_audit.py exception-safety
```

A passing run prints one summary line per audit:

```text
sync-scope audit: 4062 functions, 410 script-native roots, 0 violation(s), 0 baselined, 0 error(s)
exception-safety audit: 7701 functions checked, 0 error(s), 0 warning(s)
```

The counts change with the source. On a Windows workstation the full Engine scan takes about 12 seconds for `sync-scope` and about 4 seconds for `exception-safety`.

A `sync-scope` finding names the root, the reason and the call path:

```text
SourceExt/Bad.cpp:3: error: Server_Game_Bad reaches blocking entity-cover replacement: Server_Game_Bad -> SyncContext::SyncEntities
```

Add `--json` to `sync-scope` for a machine-readable summary, diagnostics and errors.

## Exit codes

| Code | Meaning |
|---|---|
| `0` | No finding. `exception-safety` warnings (`None` rows pending adjudication, review notes) do not fail unless `--fail-on warning` is given. |
| `1` | Audit findings: violations, classification drift, rows without reasons, stale or malformed baseline rows, stale configuration entries. |
| `2` | Invalid command line or configuration: unknown option, a scope outside the root or overlapping Engine `Source/`, a missing project baseline option, an invalid configuration file. |

## Common options

| Option | Meaning |
|---|---|
| `--root DIR` | Root for `--scope`, `--config` and `--baseline` paths. Defaults to the current directory. |
| `--engine-root DIR` | Engine checkout to audit. Defaults to the checkout that contains the script, so `Engine/BuildTools/native_safety_audit.py` audits the embedded Engine. |
| `--scope DIR` | Root-relative project native directory, scanned recursively. Repeatable. The Engine `Source/` is always audited and cannot be passed as a scope. |
| `--config FILE` | Project configuration: vendored exclusions and reviewed project rules. |
| `--baseline FILE` | Project baseline for findings in the `--scope` directories. Required by `exception-safety` when a scope is given. |

The exact parser help is in the [helper command reference](../../reference/helper-cli/commands.md).

## Maintain the exception-safety baseline

The `exception-safety` command takes an optional action: `audit` (the default), `report` or `update`. A change that touches native code follows this loop:

1. Run `exception-safety` and read the errors.
2. For a new function, run `exception-safety update --add-new`, derive its level by the [derivation rules](../../contributing/coding-contracts/exception-safety.md#8-per-function-exception-safety-classification-es-levels), and write `level`, `status` (normally `annotated`) and `reason` in the new row.
3. For a changed body, re-derive the level and reason, edit the row, then accept the new body hash with `exception-safety update --accept "Source/Client/MapView.cpp::MapView::DrawMap"`.
4. For a signature change with an unchanged body, run `exception-safety update --migrate-signatures`; for a removed function, run `exception-safety update --prune-removed`.
5. Run `exception-safety` again until it passes, and commit the source change and the baseline change together.

`report` prints the level distribution, the `None` rows and the manual-review queue; `report --level Basic --area Server` or `report --file MapView --function DrawMap` list matching rows. A level is a fact about the current body: never add `noexcept` because a row says `NoThrow`.

## Baseline formats

Each baseline is a UTF-8, tab-separated file with a fixed header row. The Engine files live in `BuildTools/audit_baselines/`; Engine rows name Engine-relative paths (`Source/...`), project rows name root-relative paths (`SourceExt/...`). An Engine row never covers a project finding and a project row never covers an Engine finding.

The `exception-safety` columns are `file`, `kind`, `function`, `signature`, `body_hash`, `level`, `status`, `review_note`, `reason`:

```text
file	kind	function	signature	body_hash	level	status	review_note	reason
SourceExt/Run.cpp	marker	Project::Run	void Project::Run()	f3a1c2d4e5b60718	Basic	annotated		Network writes can precede a later throw; the session stays valid
```

| Column | Values |
|---|---|
| `kind` | `marker` (body opens with `FO_TRACE_ZONE`), `export` (`FO_SCRIPT_API`), `markerless` (located by its stored signature), `macro_family` (one `#define` that expands to functions) |
| `body_hash` | Hash of the comment-stripped, whitespace-collapsed body with zone statements removed; written by `update`, never by hand |
| `level` | `NoThrow`, `Strong`, `Basic`, `None`, `Unclassified` |
| `status` | `annotated`, `verified`, `adjudicated`, `unclassified` |
| `review_note` | Optional standing request for manual review; reported as a warning |
| `reason` | Required sentence explaining why the body guarantees its level |

The `sync-scope` columns are `file`, `export`, `sink`, `reason`. A row accepts one reported root and sink until the code is fixed; the Engine file is empty apart from its header:

```text
file	export	sink	reason
SourceExt/LegacyDoor.cpp	Server_Door_Open	SyncContext::SyncEntity	Locks the door map while the old door script is being ported
```

A row that no longer matches a finding fails as stale, so a baseline can only shrink.

## Configuration file

The configuration is strict JSON: unknown keys, duplicate entries, wildcards in identities and entries without a reason are errors (exit `2`). Every entry that no longer matches anything fails the audit (exit `1`).

```json
{
  "schema_version": 1,
  "exclude": [
    {"path": "SourceExt/ThirdParty", "reason": "Vendored compression library, audited upstream"},
    {"path": "SourceExt/Test_*.cpp", "reason": "Native unit tests are not production code"}
  ],
  "sync_scope": {
    "independent_job_schedulers": [
      {
        "file": "SourceExt/WorkerPool.cpp",
        "function": "Server_Game_StartWorkerPool",
        "reason": "Each pool worker starts as a fresh top-level job that acquires its own cover"
      }
    ],
    "typed_factories": [
      {"name": "GetPoolServer", "type": "ServerEngine", "reason": "Returns the server that owns a worker session"}
    ],
    "deferred_callback_calls": [
      {"name": "Post", "reason": "Queues a callback on the worker pool instead of running it now"}
    ]
  }
}
```

| Key | Effect |
|---|---|
| `exclude` | Root-relative directory, file or `fnmatch` pattern removed from the project scopes, for vendored code. It applies to both audits. |
| `independent_job_schedulers` | Exact project function whose callbacks passed to a deferred call start in a fresh top-level job. Only lambdas passed to a deferred call inside that exact function are hidden from the caller's call graph. |
| `typed_factories` | Project helper whose result type the parser should assume for `auto x = Helper(...)`, so calls on `x` stay visible in the call graph. |
| `deferred_callback_calls` | Additional call names, beside the Engine's `add_job`, `run_thread`, `Submit` and `SetDataArrivedCallback`, that defer a lambda inside a reviewed scheduler. |

The Engine rules (exact Engine terminals, exempt `Game.Sync` family exports, trusted registration callers, Engine schedulers) are built in and cannot be changed by a project configuration.

## Wire the audits into a local task and CI

Run both audits after formatting and before the bake, next to the other static checks, in the order described in [Keep a Game Project Healthy](quality-gates.md#order-the-aggregate-route). In CI, a single Python step per audit is enough; no toolchain setup is required:

```bash
python3 Engine/BuildTools/native_safety_audit.py sync-scope --scope SourceExt --config Validation/NativeSafety/config.json --baseline Validation/NativeSafety/sync-scope.tsv
python3 Engine/BuildTools/native_safety_audit.py exception-safety --scope SourceExt --config Validation/NativeSafety/config.json --baseline Validation/NativeSafety/exception-safety.tsv
```

Gate on the exit code; the step prints its own summary and findings. A baseline file passed with `--baseline` to `sync-scope` must exist, even when it holds only the header. Never baseline a finding to turn a run green: fix the native code, or record a reviewed row with its reason and remove it with the fix.

## Extend it in a project

1. Keep the project files together, for example `Validation/NativeSafety/config.json`, `sync-scope.tsv` and `exception-safety.tsv`, owned by the same reviewers as the native extension code.
2. Add a thin project wrapper that fixes the arguments, so the local task, the hook and CI call one command:

   ```python
   import subprocess
   import sys
   from pathlib import Path

   ROOT = Path(__file__).resolve().parents[2]
   AUDIT = ROOT / "Engine" / "BuildTools" / "native_safety_audit.py"
   PROJECT = ["--root", str(ROOT), "--scope", "SourceExt", "--config", "Validation/NativeSafety/config.json"]

   def main() -> int:
       sync = subprocess.call([sys.executable, str(AUDIT), "sync-scope", *PROJECT, "--baseline", "Validation/NativeSafety/sync-scope.tsv"])
       exceptions = subprocess.call([sys.executable, str(AUDIT), "exception-safety", *PROJECT, "--baseline", "Validation/NativeSafety/exception-safety.tsv"])
       return max(sync, exceptions)

   if __name__ == "__main__":
       raise SystemExit(main())
   ```

3. Create the project exception-safety baseline with `exception-safety update --add-new` and the same arguments; the command writes the missing file. Classify every new row and write its reason before committing.
4. Put project-only reviewed helpers (worker schedulers, typed factories, deferred queues) in the configuration with a reason each, never in a copy of the Engine rules. A rule that no longer names a function fails, so it is removed together with the helper.
5. Put project-specific checks that are not Engine contracts in separate project scripts beside the wrapper, and call them from the same task. The Engine tool stays unmodified.
6. Keep reasons next to the rows they justify: every exclusion, configuration rule and baseline row has its own `reason` field, and an empty reason fails.

A project that kept its own earlier copy of these audits migrates in one change:

1. Delete the project copies of the sync-boundary and exception-safety scripts and their tests, and point the local task and CI at `Engine/BuildTools/native_safety_audit.py`.
2. Delete the Engine rows (`Source/...`) from the project's exception-safety baseline; the Engine baseline owns them.
3. Move the remaining project rows to the column order above (`review_note` before `reason`), write a reason for every row that has none, and pass the file with `--baseline`.
4. Move project helper names and project scheduler identities that were constants in the old script into the configuration file, and move vendored directory exclusions into `exclude`.
5. Run both audits. Fix or explain every new finding, because a recursive project scope may now include files an older non-recursive scan skipped.

## Validation checklist

- `sync-scope` and `exception-safety` both exit `0` locally and in CI with the project arguments.
- Every exclusion, configuration rule and baseline row has a written reason, and none is stale.
- The project exception-safety baseline contains project rows only; Engine rows come from the Engine checkout.
- Each native change updates its baseline rows in the same commit, with re-derived levels and reasons.
- No `sync-scope` baseline row outlives the fix it was waiting for.
