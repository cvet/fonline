# Applications and Entry Points

This document maps the app entry points in `Source/Applications/` and how they are wired by BuildTools.

## Build target ownership

Application source files are engine-owned, but target names and enablement are often project-dependent. `BuildTools/cmake/stages/Applications.cmake` creates targets from these entry points using project variables such as `FO_DEV_NAME`, platform options, and feature toggles like client/server/library/headless modes.

Because of that, this doc names source entry points and roles, not universal final target names.

## Source paths inspected

- `Source/Applications/ASCompilerApp.cpp`
- `Source/Applications/BakerApp.cpp`
- `Source/Applications/BakerLib.cpp`
- `Source/Applications/ClientApp.cpp`
- `Source/Applications/ClientLib.cpp`
- `Source/Applications/MapperApp.cpp`
- `Source/Applications/ServerApp.cpp`
- `Source/Applications/ServerDaemonApp.cpp`
- `Source/Applications/ServerHeadlessApp.cpp`
- `Source/Applications/ServerServiceApp.cpp`
- `Source/Applications/TestingApp.cpp`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/helpers/Build.cmake`

## Entry-point files

- `Source/Applications/ClientApp.cpp` — client executable host entry point.
- `Source/Applications/ClientLib.cpp` — client runtime library entry point used by host/runtime split workflows.
- `Source/Applications/ServerApp.cpp` — standard server application entry point.
- `Source/Applications/ServerDaemonApp.cpp` — daemon-style server variant.
- `Source/Applications/ServerHeadlessApp.cpp` — headless server variant.
- `Source/Applications/ServerServiceApp.cpp` — service-style server variant.
- `Source/Applications/MapperApp.cpp` — mapper tool entry point.
- `Source/Applications/BakerApp.cpp` — build/resource baking entry point.
- `Source/Applications/BakerLib.cpp` — baking library entry point when baking is composed as a library.
- `Source/Applications/ASCompilerApp.cpp` — AngelScript compiler entry point.
- `Source/Applications/TestingApp.cpp` — test runner application entry point.

## CMake wiring

`BuildTools/cmake/stages/Applications.cmake` is the primary source for application target creation. It uses helpers such as `AddExecutableApplication` and `AddSharedApplication` to create executable or shared-library outputs and to attach platform-specific properties.

Observed wiring patterns include:

- Client executable and client runtime library are created when client builds are enabled.
- Headless variants use headless frontend libraries where applicable.
- Server variants are selected by server/platform/service options.
- Mapper and batch tool applications are created only when the corresponding build options are enabled.
- Test applications are marked as testing apps so they can be treated differently from product runtime apps.

Read the CMake stage before documenting a target as available. Availability can depend on platform and project options.

## Client Startup Settings Hook

Applications that construct a `ClientEngine` call the project-provided `ClientStartupSettingsHook(GlobalSettings&, int32_t clientIndex, bool embedded)` immediately before client construction:

- `ClientApp.cpp` calls it for the standalone client with `clientIndex = 1` and `embedded = false`.
- `ServerApp.cpp` calls it for each GUI embedded client after copying app settings into a client-owned `GlobalSettings`.
- `ServerHeadlessApp.cpp` calls it for each headless embedded client after copying app settings into a client-owned `GlobalSettings`.

Server-hosted embedded clients keep separate settings objects alive for the lifetime of their `ClientEngine`. This lets an embedding project adjust per-client settings such as auth identity, AI-control ports, diagnostics, or transport toggles without mutating the server's `App->Settings` or other already-created clients. The hook is for startup-time configuration only; after `ClientEngine` construction, normal client/server authority and script-visible settings rules still apply.

## Which entry point should I inspect?

- Client startup or host/runtime behavior: `ClientApp.cpp`, `ClientLib.cpp`, [ClientUpdater.md](ClientUpdater.md).
- Server lifecycle: `ServerApp.cpp` and server variants, [ServerRuntime.md](ServerRuntime.md).
- Resource generation: `BakerApp.cpp`, `BakerLib.cpp`, [BakingPipeline.md](BakingPipeline.md).
- Script compilation: `ASCompilerApp.cpp`, [Scripting.md](Scripting.md) and [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).
- Mapper automation: `MapperApp.cpp`, [MapperTools.md](MapperTools.md).
- Tests: `TestingApp.cpp`, [../Source/Tests/README.md](../Source/Tests/README.md), the current `Source/Tests/README.md` route until Docs/Testing.md is created.

## Documentation rule

When adding or changing an application entry point:

1. Update `BuildTools/cmake/stages/Applications.cmake` or its helpers if target creation changes.
2. Update this file with the entry point's role.
3. Update a focused subsystem doc if runtime behavior changes.
4. Validate through the embedding project's relevant preset/target instead of assuming one universal target name.
