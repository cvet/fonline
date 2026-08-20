---
layout: default
title: Native Extensions
document_id: native-extensions-guide
locale: en
permalink: /Docs/en/how-to/native-extensions.html
---

# Native Extensions

Native extensions let an embedding project add C++ code to FOnline without moving game-specific behavior into the reusable engine repository. They are compiled from source as part of the same build, scanned by the same metadata/codegen pipeline, and linked into the selected engine role libraries.

Use this guide for architecture, authoring, and validation. Use the generated [role reference](../reference/native-extension/roles.md), [hook reference](../reference/native-extension/hooks.md), [binding rules](../reference/native-extension/bindings.md), and [canonical JSON model](../../generated/native-extension.json) for exact current declarations.

## Contract status

The native-extension interface is `experimental` and revision-pinned. The engine documents source composition and generated binding behavior for a pinned revision; it does not promise binary compatibility between an extension compiled against one revision and runtime libraries from another.

The engine owns:

- `AddEngineSources` role routing and source discovery;
- metadata/codegen participation for every registered source;
- supported engine hooks and generated fallbacks;
- engine namespace, pointer, nullability, and script-export conventions;
- core library/link relationships for the five roles.

The embedding project owns:

- extension implementation and state;
- third-party libraries, include paths, compile definitions, and platform availability;
- settings, persistence, database migrations, credentials, and external services;
- package payloads, signing, deployment, and release compatibility policy;
- project tests and documentation for player-visible behavior.

`FO_NATIVE_SCRIPTING` selects a scripting backend. It is not the switch for project-native extensions and must not be used as an extension-availability test.

## Source paths inspected

- `BuildTools/NativeExtensionInterface.json`
- `BuildTools/Init.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/Options.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/CoreLibs.cmake`
- `BuildTools/codegen.py`
- engine hook call sites under `Source/Applications/`, `Source/Frontend/`, `Source/Common/`, `Source/Client/`, `Source/Server/`, and `Source/Tools/`
- `Examples/MinimalProject/CMakeLists.txt`
- `Examples/MinimalProject/StarterServerExtension.cpp`
- `Examples/NativeExtensionSample/CMakeLists.txt`
- `Examples/NativeExtensionSample/SourceExt/ServerExtension.cpp`

## Build composition

Register project sources after the ThirdParty stage and before the EngineSources entrypoint:

```cmake
StartProjectGeneration()
RegisterProjectOptions()
AddThirdPartyLibraries()

AddEngineSources(
    COMMON SourceExt/CommonExtension.cpp
    SERVER SourceExt/ServerExtension.cpp
    CLIENT SourceExt/ClientExtension.cpp
    MAPPER SourceExt/MapperExtension.cpp
    BAKER SourceExt/BakerExtension.cpp
    TESTS SourceExt/Test_ProjectExtension.cpp)

RegisterEngineSources()
SetupCodeGeneration()
```

Paths and globs resolve against the embedding project's contribution root. Arguments are role/path pairs; an odd argument count or unknown role is a configure-time error. There are no separate `EDITOR`, `ANIMATION_VIEWER`, or `PARTICLE_VIEWER` source roles. Mapper, both focused viewers, Baker, and ASCompiler link `BakerLib`, so reusable authoring/baking support normally belongs in `BAKER`; code that is truly shared by all applications belongs in `COMMON`. Project-native Catch2 translation units belong in `TESTS`, which compiles them directly into enabled unit-test and coverage executables without adding them to runtime libraries.

Every resolved file is appended to both its role source list and `FO_SOURCE_META_FILES`. A header registered as `COMMON` is also added to generated common-header inputs. Register only files intended for codegen inspection: a vendored source tree belongs in a dedicated library target, not in a broad extension glob.

## Role selection

Choose the narrowest role that owns the behavior:

| Need | Role | Consequence |
|---|---|---|
| Process-wide/config/application behavior used by several applications | `COMMON` | Compiles into `CommonLib`; avoid client/server-only dependencies. |
| Authority, persistence, server networking, server script methods | `SERVER` | Compiles into `ServerLib`; unavailable to client scripts and binaries. |
| Rendering/input/client networking/client script methods | `CLIENT` | Compiles into `ClientLib`; mapper also receives client registrations through `ClientLib`. |
| Mapper-only automation or mapper script methods | `MAPPER` | Compiles into `MapperLib`. |
| Custom resource bakers and authoring support shared by Mapper/viewers/ASCompiler | `BAKER` | Compiles into `BakerLib`; `BAKER` is not a script export target. |
| Project-native Catch2 regression translation units | `TESTS` | Compiles directly into enabled unit-test and coverage executables; it has no runtime or script export target. |

Do not use `COMMON` merely to make a missing symbol link. Move the dependency to its owning role or split a small common interface from role-specific implementations.

## Project-Owned Game-System Formats

Native extensions can implement complete game-system formats without making
those formats Engine features. A typical project-owned authored system may use:

- `COMMON` for a parser, shared records, exported script types, or a registry;
- `BAKER` for syntax checks, metadata-aware validation, and generated resources;
- `SERVER`, `CLIENT`, or project scripts for authoritative runtime behavior;
- a project editor, audit tool, fixtures, and gameplay tests for authoring and
  behavioral proof.

Registration through `AddEngineSources` grants build, metadata/codegen, and
link integration. It does not transfer API, format, compatibility, security, or
documentation ownership to FOnline. The embedding project must name the parser,
baker, generated outputs, runtime consumers, validation layers, and migration
policy in its own documentation.

Do not add an Engine format guide for such a system until the reusable
implementation and tests are present in this repository. If several games need
the same system but it is not suitable for Engine core, publish a revisioned
companion repository with an exact Engine compatibility range and its own
minimal example.

## Includes and namespaces

Start with `Common.h`, then include the smallest role header needed by the implementation:

```cpp
#include "Common.h"
#include "Server.h"

FO_USING_NAMESPACE();

FO_BEGIN_NAMESPACE
///@ ExportMethod
FO_SCRIPT_API int32_t Server_Game_ProjectValue(ptr<ServerEngine> server);
FO_END_NAMESPACE

int32_t FO_NAMESPACE Server_Game_ProjectValue(ptr<ServerEngine> server)
{
    ignore_unused(server);
    return 1;
}
```

Metadata declarations must compile with the engine namespace enabled or disabled. Keep declarations inside `FO_BEGIN_NAMESPACE` / `FO_END_NAMESPACE`, and qualify definitions with `FO_NAMESPACE`.

`FO_SCRIPT_API` exports are codegen frontiers and intentionally do not start with `FO_STACK_TRACE_ENTRY()` or `FO_NO_STACK_TRACE_ENTRY()`. Ordinary non-exported project C++ functions keep the normal engine stack-trace convention.

## Script exports and metadata

Registered extension files are parsed together with engine metadata. Project code can use supported `///@` declarations such as `ExportMethod`, `ExportEvent`, `ExportRefType`, `ExportSettings`, and `EngineHook`, subject to the same parser and nullability rules as engine declarations.

The CMake source role and metadata target are related but not interchangeable:

- a `SERVER` file normally declares `Server_*` exports;
- a `CLIENT` file normally declares `Client_*` exports and those registrations are also available to mapper builds;
- a `MAPPER` file declares mapper-only exports;
- `COMMON` exports are registered on every applicable side;
- `BAKER` can implement baker hooks but is not an `ExportMethod` target.

Use `ptr<T>` / `nptr<T>` for engine handle borrows. Bare raw handle pointers are rejected by codegen. Keep argument defaults, nullability, ownership, and runtime side aligned with the generated script declaration. Rebuild and rebake after any native metadata change; do not copy generated registration files between engine revisions.

Project remote calls remain project-authored script metadata and use the baked catalog described in [Remote Calls](../reference/scripting/remote-calls.md). They are not native-extension symbols.

## Engine hooks

Hooks are optional named C++ entry points. Declare a hook with `///@ EngineHook` and the exact signature from the generated [hook reference](../reference/native-extension/hooks.md), in a file registered under its owning role. Codegen sees the declaration and omits that hook's fallback from `GenericCode-Common.gen.cpp`.

If a project does not declare a hook, codegen emits its documented fallback. Most hook presence participates in generated compatibility state; `ApplicationShutdownHook` is the current exception. Hook names are closed: an unknown name is a codegen error.

Implement each hook exactly once. Multiple declarations with one generated fallback decision can produce duplicate definitions or unresolved symbols. Keep the declaration adjacent to the implementation source and do not place ordinary comments between a `///@` tag and its declaration.

Hook bodies run at lifecycle or policy boundaries. They must preserve engine invariants and follow the owning subsystem's exception contract. In particular:

- shutdown hooks are called through guarded shutdown paths and should release resources without throwing;
- visibility hooks execute on authoritative server paths and must not introduce unsynchronized mutable global state;
- configuration hooks run while parsing settings and must return the documented changed/not-changed signal;
- baker setup should append only requested project bakers and retain shared baking context ownership correctly.

## State and lifetime

Prefer state owned by the engine instance or a project manager reachable from it. Client/server extension data can be attached through the engine's user-data ownership slot with an engine owning pointer and an explicit deleter. This keeps parallel test instances isolated and gives shutdown a deterministic owner.

Do not use file-scope mutable statics for per-engine registries, sessions, caches, or extension state. Multiple engine instances can run in one process. A process-wide service is acceptable only when its semantics are genuinely process-wide, lifecycle hooks own initialization/shutdown, and tests can isolate or disable it.

A project AiControl listener is a representative lifetime-sensitive extension:
the socket thread may parse and copy plain values, but the owning client loop
must publish observations and drain commands. Stop accepting, close sockets,
wake and join the thread, fail accepted unfinished commands, and unregister
callbacks before engine-instance state is released. The reusable envelope and
security policy are documented in [AiControl Protocol](ai-control-protocol.md);
the game's observations, actions, MCP tools, compile gate, and runtime tests stay
project-owned.

Use the engine pointer vocabulary:

- `ptr<T>` / `nptr<T>` for borrowed engine objects;
- `unique_*` / `refcount_*` and engine `shared_ptr` helpers for ownership;
- raw pointers only at documented OS/SDK ABI boundaries, wrapped immediately on entry.

## Dependencies and platforms

`AddEngineSources` does not infer dependencies. The embedding project must add libraries, include directories, compile definitions, generated headers, and platform frameworks before core libraries/applications are built. Keep third-party source in its own target and route it only to consuming roles with `AddProjectLibraries`; [Project-Local Dependencies](native-extensions/project-dependencies.md) owns the complete selection, provenance, CMake, ABI, package, and update workflow.

Platform-specific extensions need an explicit availability contract:

1. gate the real implementation with engine/project platform macros;
2. provide a compile-safe unsupported stub when a common script/native symbol must still exist;
3. reject unsupported runtime use clearly instead of silently emulating success;
4. keep package payloads and external runtime libraries synchronized with the compiled feature;
5. validate at least one enabled and one disabled build path.

Never put credentials, API keys, signing material, or private service URLs into extension source, generated metadata, examples, logs, or documentation.

## Testing strategy

Use the smallest route that proves the affected boundary:

- CMake registration/role changes: `cmake -P BuildTools/tests/validate_native_extension_interface.cmake`.
- Hook/metadata/codegen changes: regenerate and check the native-extension/API references, then run `BakeResources` in a real embedding project.
- Reusable minimal server hook: `python BuildTools/buildtools.py validate win64-starter-smoke` or `linux-starter-smoke`.
- Complete native lifecycle, role-link, script-export, and focused-test path: `python BuildTools/buildtools.py validate win64-native-extension-smoke` or `linux-native-extension-smoke`.
- Script export: add a script compile/bake assertion and a focused runtime test that calls the generated method on the correct side.
- Client-visible extension: build/run a real standalone client path; a server-only or headless smoke cannot prove rendering, input, dynamic-library, or package behavior.
- External SDK/platform bridge: exercise enabled, disabled, and packaged-runtime paths on the owning platform.

The engine-owned minimal project is the normative starter. `Examples/NativeExtensionSample` is the focused complete native path: it keeps per-server state in `ServerEngine.UserData`, routes a small library through `AddProjectLibraries`, exports one server method, and runs both a native unit and runtime smoke check. A large game project is valuable integration evidence but does not define the reusable contract.

## Updating the engine revision

Treat an Engine gitlink change as an extension compatibility event:

1. compare the old/new [canonical native-extension models](../../generated/native-extension.json) through [Generated Contract Change Management](../contributing/contract-change-management.md);
2. inspect hook signature/default/call-site, role/library, pointer/nullability, generated metadata, and compatibility-marker changes;
3. reconfigure so role validation and source globs are reevaluated;
4. rebuild every affected native role and rebake project metadata/resources;
5. update project extension docs/tests and migration/release notes where behavior changed;
6. never reuse native binaries or generated registration files from the previous Engine revision.

## Validation checklist

1. Every source is registered under the narrowest valid role before `RegisterEngineSources()`.
2. Every metadata declaration has the correct target, namespace macros, pointer/nullability vocabulary, and exact hook signature where applicable.
3. Per-engine state has an instance owner; process globals have an explicit process-wide lifecycle justification.
4. Dependencies, platform guards, disabled stubs, and package payloads match the compiled feature.
5. Generated API/native-extension references and the aggregate contract diff are current.
6. Structural CMake, codegen/bake, focused native/script tests, and the smallest real runtime path pass without warnings.
7. Project documentation records any settings, persistence, service, security, or release behavior that the engine guide intentionally excludes.

## See also

- [Embedding Project](build/embedding-project.md) - engine/game repository ownership.
- [BuildTools Pipeline](../reference/cmake-and-buildtools/pipeline.md) - stage and library composition.
- [Generated API and Metadata](../reference/metadata/index.md) - metadata and generated script API.
- [Smart Pointers](../contributing/coding-contracts/smart-pointers.md) - native pointer vocabulary.
- [Nullability](../contributing/coding-contracts/nullability.md) - native/script nullability boundary.
- [Exception Safety](../contributing/coding-contracts/exception-safety.md) - lifecycle and mutation exception contracts.
- [Project-Local Dependencies](native-extensions/project-dependencies.md) - project-local library/SDK ownership, role linking, platform/package delivery, and maintenance.
- [AiControl Protocol](ai-control-protocol.md) - project-neutral AI-control envelope, loopback threat boundary, command lifecycle, and native/MCP ownership split.
- [ThirdParty Maintenance](../contributing/third-party/index.md) - engine-vendored dependency policy.
