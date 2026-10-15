# Runtime Build Locale Implementation Plan

> **For agentic workers:** Use executing-plans to implement the checklist inline; preserve unrelated working changes.

**Goal:** Start pinned dotnet/runtime source builds reliably under non-byte host collation.

**Architecture:** Set `LC_ALL=C` in the environment passed to the POSIX runtime build child. Keep the parent environment, Windows invocation, and processor budget intact.

**Tech Stack:** Python BuildTools, pytest, Bash, host `sort` and `comm`.

**Spec:** Owner task of 2026-10-14; [Managed C# build workflow](../en/how-to/scripting/managed-csharp.md#build-and-bake-workflow).

## Status

- State: complete
- Owner: runtime-build-locale agent
- Last updated: 2026-10-14
- Engine baseline: `b497070377b3e1f8be6b9d0b91ebad71e9987a3d`; existing working version `2026.1.38-dev`.

## Global Constraints

- No commits, pushes, Authoring access, production changes, shared baking, native binary replacement, or runtime rebuild.
- Preserve other tasks' source/documentation changes and all published release history.
- Keep the reviewed AI full-context limit at 2293760 bytes; measure before writing delivery artifacts.

## Scope

- `BuildTools/buildtools.py::run_runtime_build`: child locale only.
- `BuildTools/tests/test_runtime_build_environment.py`: actual startup intersection with CI arguments, Release/Debug, explicit actions, and unchanged parent locale.
- Owning EN/RU Managed C# guide, current 38-dev changelog, internal plan classification, translation hashes, and affected delivery metadata.
- No game mechanic or player knowledge-base change; this modifies dependency preparation only.

## Review Focus

- Inherited `LC_ALL` takes precedence over `LANG` and `LC_COLLATE`.
- Explicit build/restore flags must still select the correct actions.
- Windows runtime builds must retain the existing environment.
- Processor limits and unrelated environment sanitation must remain effective.
- Updating reached build code changes the workspace runtime-cache fingerprint automatically.

## Plan

- [x] Trace the exact pinned runtime startup command and reproduce its collation failure without compiling runtime.
- [x] Add and run a RED behavioral test through real `run_runtime_build`, Bash, `sort`, and `comm`.
- [x] Set `LC_ALL=C` only in the POSIX child environment; rerun the regression GREEN.
- [x] Run existing runtime environment/budget/property/cache tests and relevant BuildTools checks.
- [x] Update EN/RU owning text and current release notes without increasing the complete AI bundle beyond its limit.
- [x] Refresh affected generated documentation in dependency order, verify scope, and request parent review.

## Evidence

Runtime `v10.0.12` resolves to `4271d88e0aebf3d04f188f1334c2220d80555ef6`. Its [eng/build.sh:159](https://github.com/dotnet/runtime/blob/4271d88e0aebf3d04f188f1334c2220d80555ef6/eng/build.sh#L159) intersects actions and arguments using `comm -12` over two sorted process substitutions, before MSBuild startup.

Local `sort` and `comm` identify as uutils coreutils 0.10.0. With the exact Linux Release/Debug CI arguments, `LC_ALL=en_US.utf8` fails with `comm: file 2 is not in sorted order` and `comm: input is not in sorted order`; `C` and `C.utf8` pass. `sort --check` accepts its own locale-sorted output. In the [upstream comm source](https://github.com/uutils/coreutils/blob/0.10.0/src/uu/comm/src/comm.rs#L102), both order validation and merge comparison use byte slices, while sort honors host collation.

The inspected CI logs prove the same startup diagnostic and pinned runtime SHA; they do not identify their coreutils implementation or locale. Local uutils evidence is not a claim about unobserved remote tools. Source discovery under local/runner Workspace found no retained dotnet/runtime checkout; primary pinned GitHub source was read without downloading or changing a checkout.

## Validation

- Initial isolated startup probe: inherited/C/C.utf8 exit 0; en_US.utf8 exit 1 with both observed diagnostics.
- RED: four Release/Debug and implicit/explicit-action cases fail at the real child invocation with the two observed `comm` diagnostics.
- GREEN: the same four cases pass with unchanged parent locale and all runtime arguments, including `/maxcpucount:26`.
- Full immutable upstream scripts in an isolated temporary tree: direct non-byte-locale `-help` calls fail in both configurations; calls through BuildTools print help and exit 0 with empty stderr. No SDK or runtime artifacts are created.
- Regression: `python -m pytest -q BuildTools/tests/test_runtime_build_environment.py -k startup_actions`.
- Existing focused BuildTools suites: runtime environment, processor budget, PowerShell properties, runtime-cache identity and restoration.
- Documentation: source byte deltas, in-memory full-context render, EN/RU hashes, focused managed documentation checks, metadata/delivery validation.
- Full runtime compilation and CI reruns remain separate qualification; no claim that every CI job shares this cause.
- Existing `test_runtime*.py` and `test_managed_runtime*.py`: 610 passed, one skipped, two failures. `test_runtime_host_cmake_does_not_inherit_the_target_sdk[ios-iPhoneOS-arm64]` and `[iossimulator-iPhoneSimulator-x86_64]` find an empty host SDK on this host. Both also fail with the published function loaded in memory without the locale assignment; no source rollback was used. They remain separate host SDK qualification issues.
- Owning managed/localization/site/AI tests: 39 passed, two subtests passed. `docs_prepare.py`, `docs_validate.py` (412 Markdown entries), `docs_ai_delivery.py --check` and `git diff --check` passed.
- Complete AI delivery: 2293756 before, 2293647 after, cap 2293760; 398 public documents and 113 bytes remaining. Published 37-dev and older changelog bodies remain byte-identical.

## Cache identity and retained proof

- Reached build-code fingerprint: `1f2f53399f1ef934b8a1b2774d5bacfbec6faa635e376ca32731d59f04299d6a` → `41e5897684b4524c4663b6308bf085133152a15a551a04aa9d620c1f1d940c4e`.
- The 15 identity parts retained in the Linux Release CI log reproduce archive suffix `7f96cd3d1a968f5295c7bdd7020e97f4`. Replacing only the build-code part predicts suffix `178c103a457f09ac837b7826e6890357`; no cache fetch, publication, or runtime rebuild was performed.
- Retained logs and SHA-256 receipts: `/home/cvet/.local/state/lf-ci-watch/20261007/ci-quest-harvest-aml3dg0x/runtime-build-locale-evidence-8bj9f2dq/receipts.json`. Original CI inputs are unchanged.
- Original tool evidence: RED terminal `8a7f35`; GREEN `3f1735`; upstream help session `94713`, terminal `a007ef`; runtime suite session `90590`, terminal `4a4bb4`; published-function iOS control terminal `8c1030`.
- Ruling: use the existing internal `_meta` plan taxonomy instead of introducing a new Engine documentation section. This keeps plans out of public routes and complete AI prose.
- Final freshness checks passed for inventory, snippets, complete EN/RU localization, generated descriptions, site/routes, AI evaluation, complete AI delivery, Markdown validation and whitespace. Retained log: `final-documentation-checks.log`, SHA-256 `cb4ea32921e644a25912ce92c88cef9ccd2c4afbf8288bb8133afa5576516b37` in the evidence directory above.
- The parent independently reviewed production/test/documentation scope and reran the four startup regression cases. No commits or pushes were made; original CI inputs and published artifacts were retained.

