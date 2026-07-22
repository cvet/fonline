# FOnline Engine — AI Maintainer Guide

This is the AI entry point for the reusable FOnline engine repository. For the human entry point, start with [README.md](README.md). For the documentation map, start with [Docs/README.md](Docs/README.md).

## Scope

- This repository is the reusable engine submodule used by games such as Last Frontier.
- Engine-owned code lives under `Source/`, `BuildTools/`, `Resources/`, and engine tests under `Source/Tests/`.
- Game-specific content, scripts, native extensions, CI glue, and launch presets live in the embedding project and should not be moved into the engine unless the behavior is genuinely reusable.

## Before Changing Anything

1. Check whether the change belongs to the engine or to the embedding game project.
2. Read the nearest existing code and follow its style; do not introduce parallel conventions.
3. Verify before editing: docs may drift, and when a doc and the live source disagree, the source wins — fix the doc in the same change.
4. If behavior changes, update the owning engine doc in `Docs/` in the same worktree change.
5. Do not commit or push unless explicitly asked by the repository owner.

## Documentation Map

The full maintained index is [Docs/README.md](Docs/README.md); use it when a topic is not listed here. Convention-critical docs for maintainers:

- [Docs/Architecture.md](Docs/Architecture.md) - engine layer map and where a behavior belongs.
- [Docs/SourceTree.md](Docs/SourceTree.md) - source-tree navigation.
- [Docs/Essentials.md](Docs/Essentials.md) - low-level platform, logging, memory, filesystem, serialization, sockets, and utilities.
- [Docs/ConfigurationAndDataSources.md](Docs/ConfigurationAndDataSources.md) - config parsing, settings, data sources, file lookup, and caches.
- [Docs/Testing.md](Docs/Testing.md) - test-suite inventory, generated test targets, coverage, and validation routing.
- [Docs/DocumentationMaintenance.md](Docs/DocumentationMaintenance.md) - source-grounded docs maintenance workflow.
- [Docs/ThirdPartyMaintenance.md](Docs/ThirdPartyMaintenance.md) - vendored dependency update, pruning, version pin, and `(FOnline Patch)` workflow.
- [Docs/ClientUpdater.md](Docs/ClientUpdater.md) - client host/runtime split, ABI, updater protocol, and `UpdaterBackend`.
- [Docs/Debugging.md](Docs/Debugging.md) - stack traces, debugger helpers, native debugging, and validation notes.
- [Docs/Nullability.md](Docs/Nullability.md) - `T?` script / `ptr<T>`·`nptr<T>` native boundary contract.
- [Docs/SmartPointers.md](Docs/SmartPointers.md) - native smart-pointer vocabulary (`ptr<T>`/`nptr<T>` borrows, `unique_*`/`refcount_*` owners, engine-own `shared_ptr`/`weak_ptr`), raw-pointer allowlist, and audit expectations.
- [Docs/ExceptionSafety.md](Docs/ExceptionSafety.md) - engine-invariant stability under exceptions: terminate-on-OOM allocation model, entity-lifecycle throw-as-signal contract, and the `FO_STRONG_ASSERT` disposition rules.
- [Docs/ThreadSafetyAnalysis.md](Docs/ThreadSafetyAnalysis.md) - `FO_TSA_*` Clang Thread Safety Analysis annotations, locking primitives, and `-Werror=thread-safety` enforcement.
- [Docs/MapperTools.md](Docs/MapperTools.md) - mapper automation and native mapper helper integration points.
- [Docs/WebDebugging.md](Docs/WebDebugging.md) - web target build/debug workflow.
- [Docs/AndroidDebugging.md](Docs/AndroidDebugging.md) - Android target build/debug workflow.
- [PUBLIC_API.md](PUBLIC_API.md) - public API notes.
- [TUTORIAL.md](TUTORIAL.md) - engine tutorial.
- [Source/README.md](Source/README.md) - source-tree overview.
- [Source/Tests/README.md](Source/Tests/README.md) - engine unit-test suites.
- [BuildTools/README.md](BuildTools/README.md) - build-tooling notes.

## Validation Routing

- **Zero tolerance for warnings.** Engine C++ builds and codegen must finish clean; there is no acceptable warning backlog (Clang thread-safety analysis already runs as `-Werror=thread-safety`). Never introduce a new warning; if a change surfaces one, resolve it at the root in the same change rather than suppressing it.
- Engine C++ changes: build and run the engine unit-test target used by the embedding project (`LF_UnitTests` / `RunUnitTests` in Last Frontier).
- Build-system changes: validate the affected CMake preset or BuildTools command in the embedding project that exercises it.
- Platform-packaging changes: validate the relevant package path (`Raw`, `Raw+WebServer`, Android package, etc.) and update the platform doc.
- Script/native API boundary changes: update nullability/API docs and run the smallest test target that covers the changed binding.
- When a serialized contract changes (entity properties, network messages, save data), update the relevant `///@ MigrationRule` metadata.
- Engine changes that affect network interaction or are otherwise substantial enough to matter for client/server runtime compatibility must force a compatibility-version change by bumping the central marker in `Source/Common/Common.h`:

  ```cpp
  // Force change of compatability version
  ///@ MigrationRule Version 0 0 5
  ```

## Behavioral Rules

- **Fix the source, never mask.** When a lower layer returns a wrong result, swallows an error, or silently coerces bad input into "looks fine", fix it at the source (throw / validate / propagate) instead of papering over it from the caller with pre-checks, try/catch that fabricates a "good" value, default substitutes, or other workarounds. Do not leave legacy code paths, dead branches, deprecated overloads, compatibility shims, or "just in case" fallbacks unless explicitly requested. If you find yourself adding a guard that compensates for a bug elsewhere, stop and fix the bug instead.
- **Keep engine invariants stable under exceptions.** An exception thrown partway through a multi-step engine mutation (entity create/register/destroy/invalidate, cross-entity links) must never leave a half-mutated, restart-only state. Engine allocation terminates on OOM (`SafeAlloc`/`SafeAllocator`), so `bad_alloc` is not a recoverable error and never a reason to build a rollback. A post-mutation invariant that "can only be false if the world is already corrupt" is a `FO_STRONG_ASSERT` (always-on, deterministic exit), not a swallowable `FO_VERIFY_AND_THROW`. Entity create/destroy is throw-as-signal, not transactional rollback (events may legitimately destroy or relocate the entity; the function throws but the entity is left in a valid state) — pinned by `Source/Tests/Test_EntityLifecycle.cpp` and `Test_ServerMapOperations.cpp`, so do not add a blanket create-time rollback. Full rules: [Docs/ExceptionSafety.md](Docs/ExceptionSafety.md).
- **No singletons. No hidden global / static state for engine-semantic data.** State with *per-engine semantics* (lock sets, entity registries, ticket allocators tied to one engine's wait queues, mutable runtime caches that could be mixed across engines) lives on an owning instance reachable through the engine object graph — typically `ServerEngine` (managers), `ServerEntity` (per-entity locks, properties, parent), or `BaseEngine` (`ScriptSystem`, settings). Do not introduce file-scope mutable statics, `static` data members on engine classes, function-local statics that cache cross-call state, or unnamed-namespace mutable variables. Multiple engine instances may coexist in one process (embedding projects run parallel test suites this way), so hidden globals silently share state across engines, hide cross-test pollution, and turn lifetime-ordering bugs into timing windows.
- **`static thread_local` is OK *only* when threads are partitioned by engine ownership** and the slot can never observe values from a foreign engine on the same thread. The current example is `SyncContext* CurrentContext` in `Source/Server/EntitySync.cpp`: every thread that touches it belongs to exactly one engine, so the thread-local slot is implicitly per-engine. The same logic permits a `static std::atomic` whose value has no per-engine semantics (e.g. a monotonic ticket counter only ever compared inside one lock's wait queue). When in doubt, prefer instance ownership — but do not pay a mutex-plus-map lookup just to satisfy the rule when a `thread_local T*` is already correctly isolated.
- **`static` is otherwise acceptable only for** compile-time constants (`static constexpr`), file-local pure helper functions (`static void Helper(...)`), and `static_assert`. An existing static that holds per-engine state is a bug to migrate to instance ownership, not a precedent to copy.

## Style Notes

- Prefer existing engine idioms over new local abstractions.
- Reuse the existing MIT source-file header and `#pragma once`; match the surrounding module-level layout.
- Module layout: put class definitions and static-function forward declarations near the top of the translation unit, then implementations ordered from high-level entry points down to low-level helpers, so a reader meets the public/orchestrating code first.
- Order code top-down by importance and abstraction level: high-level entry points and orchestration first, secondary helpers and low-level details below, unless a nearby file has a stronger established ordering.
- First statement of every `.cpp` method/function body: `FO_STACK_TRACE_ENTRY()` or `FO_NO_STACK_TRACE_ENTRY()`. If the body continues after the marker, keep one blank line after it. Do not put either macro in headers, inline header-defined bodies, or lambda bodies. **Exception: `FO_SCRIPT_API` script-export functions (`///@ ExportMethod`, `///@ EngineHook`, and the other script-bound exports) take *no* stack-trace marker at all — the body starts directly after the opening brace.**
- Separate semantically distinct code blocks with a blank line. In particular, surround every control-flow statement (`if`/`else`, `for`, `while`/`do`, `switch`, `try`/`catch`, and similar constructs) with a blank line before and after the complete statement. Blank lines may be omitted only inside a consecutive run of the same construct (`if` + `if`, `for` + `for`, and so on).
- Treat preprocessor directives as transparent to the blank-line rule: do not add or remove blank lines merely because code is enclosed by `#if`/`#else`/`#endif`; apply spacing to the surrounding C++ blocks as if the directives were not there.
- Keep a comment attached to the code block it describes below: put a blank line before the comment, but no blank line between the comment and that block. For control-flow spacing, the comment is part of the following block.
- Prefer `FO_VERIFY_AND_THROW` over silently masking unexpected states.
- Use `numeric_cast` for numeric conversions; do not use `static_cast` for numeric narrowing/widening unless the surrounding code has a specific established reason.
- Use fixed-width types (`int8_t`, `uint8_t`, `int16_t`, `uint16_t`, `int32_t`, `uint32_t`, `int64_t`, `uint64_t`, `float32_t`, `float64_t`, `size_t`) instead of bare `int` / `float` in new engine code.
- Do not use `auto` for primitive values or simple obvious types such as `string`, `hstring`, `size_t`, and the fixed-width aliases.
- Use the engine smart-pointer vocabulary from `Source/Essentials/SmartPointers.h` — `ptr<T>`/`nptr<T>` for borrows, `unique_*`/`refcount_*` for owners, engine-own `shared_ptr`/`weak_ptr` via `SafeAlloc::MakeShared()` — instead of `std::` smart pointers or bare raw `T*`. Raw pointers remain only at the documented ABI/low-level allowlist boundaries; inside a function, bind them to wrappers before ordinary engine work and unwrap with `.get()` only at the final handoff. See [Docs/SmartPointers.md](Docs/SmartPointers.md); embedding projects may gate this with an audit tool (Last Frontier: `Tools/SmartPointerAudit/`).
- For `Entity` and derived types use **pointers**, not references.
- Prefer `static` free functions for file-local helpers instead of unnamed namespaces.
- Do not add `static` variables for hidden state or caches; see the Behavioral Rules above for the narrow allowed uses of `static`.
- Add `const` and `noexcept` where they express the semantic contract; do not add them mechanically everywhere possible.
- Use `ignore_unused(...)` only for variables/objects; for an intentionally ignored function-call result, write `(void)FunctionCall(...)`.
- For C++ string/text construction and parsing, prefer existing engine helpers such as `strex` and `strvex` when they make formatting or token handling clearer. If the helper surface is missing a repeated string-formatting operation, add a reusable helper in the appropriate engine utility layer instead of open-coding ad hoc parsing/formatting at call sites.
- Gate conditional compilation on our own (`FO_*` / project) macros with `#if FOO` / `#if !FOO`, never `#ifdef` / `#ifndef`: every such macro is mandatorily `#define`d to `0` or `1`, so its definedness must never carry meaning — only its value does.
- Include layering: non-Essentials engine code must consume Essentials modules through `Common.h`, not by including individual `Source/Essentials/*.h` headers directly. STL headers are centralized through `BasicCore.h`; add or adjust standard-library includes there instead of including `<...>` headers from higher layers.
- **Essentials layering is strict.** The `Source/Essentials/Essentials.h` aggregate lists Essentials headers in include order — each Essentials header (and its `.cpp`) may only depend on headers listed *above* it in that block. If a low-layer module needs information that physically lives in a higher layer, expose only what the lower layer can produce on its own or take the higher-layer value as a parameter — do not back-channel the include and do not reorder the block to work around it.
- **Iterating server-side entities while events may fire:** any script-visible event (e.g. `OnItemOnMapAppeared.Fire(...)`, `OnCritterDisappeared.Fire(...)`) can re-enter scripts and mutate world state — including destroying the very entity being iterated. The convention is: (1) snapshot the iteration set with `copy_hold_ref(...)` so each element is held by ref-count for the duration of the loop; (2) re-validate inside the loop with `if (cr->IsDestroyed()) continue;` (and after each event for any other entity you keep working with); (3) fire scripted events at the **end** of the unit of work, after non-revocable state changes are committed.
- Keep edited source files ending with exactly one trailing blank line.
- Keep docs reusable: describe engine behavior first; mention Last Frontier only as an embedding-project example, never as an engine-doc dependency or validation owner.
- Keep `README.md` human-oriented and `AGENTS.md` AI-oriented. `CLAUDE.md` is intentionally only a pointer to `@AGENTS.md`.
