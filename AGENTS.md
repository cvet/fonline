# FOnline Engine — AI Maintainer Guide

This is the AI entry point for the reusable FOnline engine repository. For the human entry point, start with [README.md](README.md). For the documentation map, start with [Docs/README.md](Docs/README.md).

## Scope

- This repository is the reusable engine submodule used by games such as Last Frontier.
- Engine-owned code lives under `Source/`, `BuildTools/`, `Resources/`, and engine tests under `Source/Tests/`.
- Game-specific content, scripts, native extensions, CI glue, and launch presets live in the embedding project and should not be moved into the engine unless the behavior is genuinely reusable.

## Before Changing Anything

1. Check whether the change belongs to the engine or to the embedding game project.
2. Read the nearest existing code and follow its style; do not introduce parallel conventions.
3. If behavior changes, update the owning engine doc in `Docs/` in the same worktree change.
4. Do not commit or push unless explicitly asked by the repository owner.

## Documentation Map

- [Docs/Essentials.md](Docs/Essentials.md) - low-level platform, logging, memory, filesystem, serialization, sockets, and utilities.
- [Docs/ConfigurationAndDataSources.md](Docs/ConfigurationAndDataSources.md) - config parsing, settings, data sources, file lookup, and caches.
- [Docs/Testing.md](Docs/Testing.md) - test-suite inventory, generated test targets, coverage, and validation routing.
- [Docs/DocumentationMaintenance.md](Docs/DocumentationMaintenance.md) - source-grounded docs maintenance workflow.
- [Docs/ClientUpdater.md](Docs/ClientUpdater.md) - client host/runtime split, ABI, updater protocol, and `UpdaterBackend`.
- [Docs/Debugging.md](Docs/Debugging.md) - stack traces, debugger helpers, native debugging, and validation notes.
- [Docs/Nullability.md](Docs/Nullability.md) - `T?` / `FO_NULLABLE` script/native boundary contract.
- [Docs/MapperTools.md](Docs/MapperTools.md) - mapper automation and native mapper helper integration points.
- [Docs/WebDebugging.md](Docs/WebDebugging.md) - web target build/debug workflow.
- [Docs/AndroidDebugging.md](Docs/AndroidDebugging.md) - Android target build/debug workflow.
- [PUBLIC_API.md](PUBLIC_API.md) - public API notes.
- [TUTORIAL.md](TUTORIAL.md) - engine tutorial.
- [Source/README.md](Source/README.md) - source-tree overview.
- [Source/Tests/README.md](Source/Tests/README.md) - engine unit-test suites.
- [BuildTools/README.md](BuildTools/README.md) - build-tooling notes.

## Validation Routing

- Engine C++ changes: build and run the engine unit-test target used by the embedding project (`LF_UnitTests` / `RunUnitTests` in Last Frontier).
- Build-system changes: validate the affected CMake preset or BuildTools command in the embedding project that exercises it.
- Platform-packaging changes: validate the relevant package path (`Raw`, `Raw+WebServer`, Android package, etc.) and update the platform doc.
- Script/native API boundary changes: update nullability/API docs and run the smallest test target that covers the changed binding.
- Engine changes that affect network interaction or are otherwise substantial enough to matter for client/server runtime compatibility must force a compatibility-version change by bumping the central marker in `Source/Common/Common.h`:

  ```cpp
  // Force change of compatability version
  ///@ MigrationRule Version 0 0 5
  ```

## Style Notes

- Prefer existing engine idioms over new local abstractions.
- Prefer `static` free functions for file-local helpers instead of unnamed namespaces.
- Order code top-down by importance and abstraction level: high-level entry points and orchestration first, secondary helpers and low-level details below, unless a nearby file has a stronger established ordering.
- Use `ignore_unused(...)` only for variables/objects; for an intentionally ignored function-call result, write `(void)FunctionCall(...)`.
- For C++ string/text construction and parsing, prefer existing engine helpers such as `strex` and `strvex` when they make formatting or token handling clearer. If the helper surface is missing a repeated string-formatting operation, add a reusable helper in the appropriate engine utility layer instead of open-coding ad hoc parsing/formatting at call sites.
- Include layering: non-Essentials engine code must consume Essentials modules through `Common.h`, not by including individual `Source/Essentials/*.h` headers directly. STL headers are centralized through `BasicCore.h`; add or adjust standard-library includes there instead of including `<...>` headers from higher layers.
- Keep docs reusable: describe engine behavior first; mention Last Frontier only as an embedding-project example, never as an engine-doc dependency or validation owner.
- Keep `README.md` human-oriented and `AGENTS.md` AI-oriented. `CLAUDE.md` is intentionally only a pointer to `@AGENTS.md`.
