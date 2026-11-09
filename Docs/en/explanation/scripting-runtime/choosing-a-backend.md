---
layout: default
title: Choose a Script Backend
locale: en
document_id: choosing-a-script-backend
permalink: /Docs/en/explanation/scripting-runtime/choosing-a-backend.html
---

# Choose a Script Backend

An embedding project selects its gameplay scripting backend when it configures the build: AngelScript (`.fos` modules) or Managed C# (`.cs` modules on embedded Mono). The choice changes toolchain prerequisites, sanitizer lanes, debugging, static analysis, test style and the server concurrency contract, and it is cheap to make before the first script and expensive to reverse afterwards. This page lists what each backend needs, where they differ, and how to set each one up; the owning guides carry the full contracts.

## Source paths inspected

- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/ProjectOptions.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/buildtools.py`
- `BuildTools/README.md`
- `BuildTools/SupportMatrix.json`
- `Source/Common/Settings.inc`
- `Source/Server/Server.cpp`
- `Source/Server/EntitySync.cpp`
- `Source/Tests/Test_ServerEngine.cpp`
- `Docs/en/explanation/scripting-runtime/index.md`
- `Docs/en/explanation/runtime/server.md`
- `Docs/en/how-to/scripting/managed-csharp.md`
- `Docs/en/how-to/scripting/style-and-refactoring.md`
- `Docs/en/how-to/scripting/lifecycle-and-concurrency.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/troubleshooting/debugging.md`
- `Docs/en/reference/platforms/support-matrix.md`
- `Docs/en/reference/script-api/settings.md`
- `Docs/en/contributing/coding-contracts/nullability.md`
- `Examples/MinimalProject/`

## Short answer

Both implemented backends share the same metadata, entity model, events, remote calls and native export surface, so a game design does not depend on the choice. Native scripting (`FO_NATIVE_SCRIPTING`) is only a reserved option: its source root contains a single placeholder file and its CMake description says no Native backend is implemented. Do not select it.

| If the project needs | Prefer | Why (source of the claim) |
| --- | --- | --- |
| A maintained, runnable starting point | AngelScript | All five projects under `Examples/` ship `.fos` modules and set `FO_ANGELSCRIPT_SCRIPTING ON`. The Engine ships no managed starter example yet: no `.cs` file is tracked under `Examples/`. |
| The smallest toolchain | AngelScript | Managed C# additionally needs a .NET SDK on the build host and builds an embedded Mono runtime from source on the first build. |
| MemorySanitizer or ThreadSanitizer lanes with scripts enabled | AngelScript | `Init.cmake` rejects `San_Memory*` and `San_Thread` when `FO_MANAGED_SCRIPTING` is on. |
| Live script stepping in an editor | AngelScript | The Engine ships an opt-in AngelScript debugger endpoint. It has no C# counterpart. |
| Compile-time proof of server entity-cover use | Managed C# | The Roslyn analyzer (`FOSYNC` diagnostics) is part of the managed build; AngelScript relies on runtime cover checks. |
| `Task`/`await`, Roslyn tooling, NuGet analyzers, IDE navigation | Managed C# | The baker generates a `.gen.csproj`/`.gen.sln` that IDEs open and the build treats warnings as errors. |
| Replacing a script method in a running process | Managed C# | Patch-point weaving and the live compiler exist only in the managed backend, and both are opt-in. |

If none of these rows decides the question, start with AngelScript, because the runnable examples, the support matrix's compiler lanes and the debugger exist for it.

## What each backend requires

| Item | AngelScript | Managed C# |
| --- | --- | --- |
| CMake option | `FO_ANGELSCRIPT_SCRIPTING` | `FO_MANAGED_SCRIPTING` |
| Source files | project `.fos` modules | project top-level `.cs` files plus Engine `CoreScripts` |
| Extra build application | `<DevName>_ASCompiler`, built when `FO_BUILD_ASCOMPILER` is on; configuration fails if that option is on without AngelScript | `<DevName>_ManagedScriptBaker`, built when the baker library is built (`FO_BUILD_BAKER`, `FO_BUILD_MAPPER`, `FO_BUILD_ASCOMPILER` or `FO_UNIT_TESTS`) |
| Build host additions | none beyond the normal C++ toolchain | a .NET SDK able to run `ManagedScript.MsBuild` (default `dotnet msbuild`) for `ManagedScript.TargetFramework` (default `net10.0`), and the Mono runtime build described below |
| Resource pack baker | `Bakers = AngelScript` | `Bakers = Managed` |
| Project settings | `AngelScript.*` group | `ManagedScript.*` group |
| Compile artifact | baked AngelScript bytecode | per-role assemblies (`<Pack>.Server.dll`, `<Pack>.Client.dll`, `<Pack>.Mapper.dll`) plus a runtime payload |
| Fast compile check | `CompileAngelScript` | `CompileManagedScripts` |

The options are independent booleans. A production project enables exactly one; cross-backend invocation tests are the documented reason to enable more. Select them in the project `CMakeLists.txt` the way `Examples/MinimalProject/CMakeLists.txt` does:

```cmake
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_MANAGED_SCRIPTING OFF)
```

A fully managed project inverts the first and third lines. `SetOption` only assigns a variable that is not yet defined and never checks the name against `ProjectInterface.json`, so a misspelled option passes silently; verify spelling against [CMake Options](../../reference/cmake/options.md).

## Platform, sanitizer, debugging, analysis and tests

| Concern | AngelScript | Managed C# |
| --- | --- | --- |
| Engine-gated lanes | The required desktop lanes of the [support matrix](../../reference/platforms/support-matrix.md) build the AngelScript compiler (`win64-ascompiler`, `linux-ascompiler`). | The only dedicated BuildTools validation names are `managed-mac-client`, `managed-ios-simulator-client` and `managed-ios-device-client`, run through the manual `managed-apple` workflow job; they link a client and execute nothing. |
| Platform reach | The vendored runtime has patched native-call paths for x64 GCC, x64 MSVC and ARM64; check any other target yourself. | Wired for Windows, Linux, Android, WebAssembly, macOS and iOS build paths. That is source capability, not a release claim: qualify every shipped target with your own pack, payload, startup and update route. |
| Host limits | none beyond the normal toolchain | The runtime for a Windows target cannot be built on a non-Windows host: configuration aborts unless `FO_MANAGED_RUNTIME_PREBUILT` (environment) points at a tree built on Windows. |
| Sanitizers | Clang adds `San_Address`, `San_Memory`, `San_MemoryWithOrigins`, `San_Undefined`, `San_Thread`, `San_DataFlow` and `San_Address_Undefined`. | `San_Memory*` and `San_Thread` are not offered and abort configure when forced. AddressSanitizer and undefined-behavior lanes remain, and need your own managed runtime checks. |
| Live debugging | Opt-in endpoint: `AngelScript.DebuggerEnabled = True`, loopback bind by default, line breakpoints, stepping, read-only locals. Development configs only. | No Engine editor adapter or launch profile. Use the generated solution for navigation, Roslyn diagnostics, runtime logs and focused tests; attach a native or managed debugger only after matching assemblies, payload and symbols. |
| Static analysis | Compiler rules for attributes, direct calls of dispatcher-owned functions, const-only globals and `T?` nullability. Declaration-parity checks beyond that are project-side. | Nullable references, `TreatWarningsAsErrors`, checked arithmetic, `ManagedScript.Analyzers`/`AnalyzerPackages`, and the `FOSYNC` cover analyzer. |
| Formatting | Engine wrapper `buildtools.py format-source` requires clang-format 20 and repairs `.fos` syntax, but only formats Engine `Source`; the project wires its own `.fos` formatting. | Project-owned formatter and style gate; the generated project enforces code style in the build. |
| Engine test coverage to read | `Test_AngelScript*.cpp` | `Test_ManagedScriptBaker.cpp`, `Test_ManagedScriptRuntime.cpp`, `BuildTools/tests/test_managed_*.py`, analyzer and CoreScripts test projects |
| Project script tests | project-owned harness, run through the [gameplay test runner](../../how-to/testing/gameplay-and-integration.md) | the same runner contract; the harness is C# and is project-owned |

Read the [debugging guide](../../troubleshooting/debugging.md) before relying on an attach workflow, and the [nullability contract](../../contributing/coding-contracts/nullability.md) for the `T?` rules both backends share.

## Build and bake side by side

Both routes depend on `ForceCodeGeneration`, so script-visible metadata is current before any script compiles. The commands are the same in PowerShell and bash.

| Step | AngelScript | Managed C# |
| --- | --- | --- |
| Prepare host | normal C++ toolchain only | install a .NET SDK; optionally pre-stage the pinned runtime source with `py -3 Engine/BuildTools/buildtools.py prepare-workspace dotnet` (Windows) or `python3 Engine/BuildTools/buildtools.py prepare-workspace dotnet` (Linux) |
| Configure | `cmake --preset <your-preset>` with AngelScript on | the same, with `FO_MANAGED_SCRIPTING` on; the `SetupManagedRuntime` target, a dependency of the generated outputs, builds Mono from the revision pinned in `ThirdParty/dotnet-runtime` on the first build |
| Compile scripts | `cmake --build <build-dir> --config RelWithDebInfo --target CompileAngelScript` | `cmake --build <build-dir> --config RelWithDebInfo --target CompileManagedScripts` |
| What it runs | `<DevName>_ASCompiler -ApplyConfig <project .fomain> -ApplySubConfig NONE` | `<DevName>_ManagedScriptBaker` with the same arguments: generates the C# API, `.gen.csproj` and `.gen.sln`, then compiles assemblies |
| Bake | `BakeResources` / `ForceBakeResources`; the `AngelScript` baker writes bytecode into the pack | the same targets; the `Managed` baker packs assemblies and `ManagedRuntime/` |
| Failure signal | compiler diagnostics with script file and line | `ManagedScriptBakerException` for unsupported shapes, MSBuild/Roslyn diagnostics otherwise |

A bake is the only step that proves delivery. Compiling scripts proves syntax and metadata agreement; it does not prove that the resource pack, runtime payload or startup path works.

The `.fomain` carries the backend choice as a resource pack, so keep it consistent with the CMake option:

```ini
[ResourcePack]
Name = Scripts
InputDirs = Scripts
Bakers = AngelScript
ServerOnly = True
```

For Managed C#, select the pack and the generated-project settings together; the settings below are the documented defaults plus the analyzer path shown in the project-configuration guide:

```ini
ManagedScript.Assemblies = MyGame
ManagedScript.ProjectName = MyGame
ManagedScript.TargetFramework = net10.0
ManagedScript.MsBuild = dotnet msbuild
ManagedScript.Dirs = Engine/Source/Scripting/Managed/CoreScripts Scripts
ManagedScript.Analyzers = Engine/Source/Scripting/Managed/Analyzers/FOnline.Analyzers.csproj

[ResourcePack]
Name = ManagedScripts
InputDirs = Engine/Source/Scripting/Managed/CoreScripts Scripts
IncludePatterns = *
Bakers = Managed
```

See [Project Configuration](../../how-to/build/project-configuration.md#define-resource-packs) for all pack fields, [Generated Content Workflow](../../how-to/build/generated-content.md#compile-scripts) for the compile routes, and the [`ManagedScript` settings table](../../how-to/scripting/managed-csharp.md#configure-the-backend) for every managed setting.

On Windows, `dotnet/runtime` paths sit close to `MAX_PATH`. The CMake stage documents `FO_DOTNET_DIR` as the override: set it, in CMake or the environment, to a short path before the first managed build.

## Concurrency choice

Server scripts normally run on a worker pool, so every script entry must prove it holds the entities it touches. AngelScript uses runtime cover operations (`Game.Sync`, `Game.Lock`); Managed C# uses `Sync` helpers proved by the `FOSYNC` analyzer. See [Script Lifecycle and Concurrency](../../how-to/scripting/lifecycle-and-concurrency.md#server-entity-synchronization) for the contract.

`Server.SingleThreadedLogic` is the opt-out. The setting is read-only after startup, server-scoped and `false` by default (see the [settings reference](../../reference/script-api/settings.md)). With it enabled:

- the server worker pool is created with one thread, and the setting takes precedence over `Server.WorkerThreads`;
- entity-access validation accepts every entity and the cover-acquisition calls acquire nothing, so `Game.Sync` and `Game.SyncRelease` do nothing;
- `Game.Lock` and `Game.Unlock` still take the engine singleton bucket, which is uncontended;
- entity lifetime checks remain: a handle captured in one job may refer to an entity destroyed before a later job runs;
- the mode is read per engine instance, so differently configured servers can coexist in one process.

| Choose | When | Cost |
| --- | --- | --- |
| Default (`false`) | The game needs logic to use several cores, or scripts will keep their synchronization calls for a later switch. | Every script entry must build a complete, correct cover; mistakes are contract errors, not benign races. |
| `Server.SingleThreadedLogic = True` | The game is small enough for one logic worker, or the project prefers serial semantics over parallelism. | All game logic shares one worker thread, so parallel scaling of logic is given up. The setting is fixed for a server run. |

Disabling the setting later restores the full cover contract without changing scripts that kept their ordinary synchronization calls, but scripts written only for serial mode may lack them. For Managed C#, the Roslyn analyzer sources contain no reference to the setting, so if you configure the analyzer, cover annotations still gate the build in either mode. The opt-out is backend-neutral and is pinned by `ServerEngineSingleThreadedLogicRunsWithoutEntityCover` in `Source/Tests/Test_ServerEngine.cpp`; the [server runtime](../runtime/server.md#entity-synchronization-and-locking) page owns the full description.

## Migrating between backends

Moving a project from AngelScript to Managed C# is a port of declarations, lifecycle, callbacks, named calls, synchronization, tests, resource packs and packages, not a rename of files. Deleting `.fos` files without removing the AngelScript baker pack and adding the `Managed` pack leaves no active gameplay scripts. Keep public metadata names, remote-call declarations, property layouts and persisted identifiers unchanged unless the migration deliberately changes them, and do not ship two handlers for one event by accident. The mapping of attributes (`[[ModuleInit]]` to `[ModuleInit]`, `[[Async]]`/`Yield` to `Task`/`ScriptTask.Delay`, runtime cover to `[RequiresCover]` and friends) is in the [Managed C# migration section](../../how-to/scripting/managed-csharp.md#migration-from-angelscript). The reverse direction has no Engine guide; treat it as the same checklist applied backwards.

## Setup checklist: AngelScript

1. Set `FO_ANGELSCRIPT_SCRIPTING ON` and `FO_MANAGED_SCRIPTING OFF` in the project `CMakeLists.txt`; enable `FO_BUILD_ASCOMPILER` in the preset or `-D` arguments.
2. Add a `[ResourcePack]` with `Bakers = AngelScript` for the script directory, as `Examples/MinimalProject/FOnlineStarter.fomain` does.
3. Decide `AngelScript.*` project settings, for example the namespaces allowed to hold mutable globals; keep them minimal.
4. Wire a `.fos` formatter that preserves nullable syntax, and pin clang-format 20 ([formatter contract](../../how-to/scripting/style-and-refactoring.md#formatter-contract)).
5. Compile every role the sources reach with `CompileAngelScript`, then run `BakeResources`.
6. Decide `Server.SingleThreadedLogic` and record it with the project's other startup settings.
7. Add the project script tests and the [process runner](../../how-to/testing/gameplay-and-integration.md) lane; use `AngelScript.DebuggerEnabled` only in development configs.

## Setup checklist: Managed C#

1. Install a .NET SDK that builds `ManagedScript.TargetFramework`, and pin it in the project so every machine and CI runner uses the same one.
2. Set `FO_MANAGED_SCRIPTING ON` and `FO_ANGELSCRIPT_SCRIPTING OFF`; enable the baker or another option that builds the baker library.
3. Optionally pre-stage the runtime source with `prepare-workspace dotnet` and set a short `FO_DOTNET_DIR` on Windows.
4. Add the `ManagedScript.*` settings and a `Managed` resource pack that includes Engine `CoreScripts` and the project directory.
5. Configure `ManagedScript.Analyzers` and treat analyzer warnings as build failures.
6. Run `CompileManagedScripts`, then `BakeResources`; confirm the pack holds `Assemblies/Assemblies-<target>/` and `ManagedRuntime/`.
7. Decide `Server.SingleThreadedLogic`, and ban `Task.Run`, `ConfigureAwait(false)` and ThreadPool escapes statically.
8. Keep sanitizer lanes to AddressSanitizer or undefined-behavior configurations.
9. Qualify each shipped platform with its own runtime payload; never reuse one target's payload for another.
10. Document the backend options, SDK pin, source roots, analyzers and exact commands in the project, as the [project documentation boundary](../../how-to/scripting/managed-csharp.md#project-documentation-boundary) requires.

## Validation checklist

- [ ] Exactly one of `FO_ANGELSCRIPT_SCRIPTING` and `FO_MANAGED_SCRIPTING` is on; `FO_NATIVE_SCRIPTING` is off.
- [ ] The resource pack baker (`AngelScript` or `Managed`) matches the CMake option.
- [ ] The compile target for the chosen backend passes for every role without warnings.
- [ ] `BakeResources` completes and the server starts from the baked output.
- [ ] `Server.SingleThreadedLogic` is set deliberately and documented.
- [ ] Sanitizer lanes in CI exist only for configurations the backend allows.
- [ ] Platform claims are backed by project acceptance, not by Engine source capability.
