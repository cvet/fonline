---
layout: default
title: Configure a Game Project
locale: en
document_id: project-configuration
permalink: /Docs/en/how-to/build/project-configuration.html
---

# Configure a Game Project

This guide shows how an embedding project should author its `.fomain` file, resource packs, and named sub-configs. Use [Configuration and Data Sources](../../reference/settings/configuration-and-data-sources.md) for the exact runtime model and [generated settings reference](../../../generated/api/settings.md) for current built-in setting names.

## Source paths inspected

- `Source/Common/Settings.h`
- `Source/Common/Settings.cpp`
- `Source/Common/Settings.inc`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Tests/Test_Settings.cpp`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Examples/MinimalProject/CMakeLists.txt`
- `Examples/MinimalProject/FOnlineStarter.fomain`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`

## Start from an executable baseline

Copy structure from [MinimalProject](../../../../Examples/MinimalProject/README.md) for a headless first run or [MinimalMultiplayer](../../../../Examples/MinimalMultiplayer/README.md) for a client/server game. Keep the game repository as the CMake root and set the master config explicitly:

```cmake
include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "MyGame.fomain")
SetOption(FO_DEV_NAME "MYGAME")
SetOption(FO_NICE_NAME "My Game")
```

`FO_MAIN_CONFIG` is a configure-time project option. The `.fomain` contents are runtime and baking settings. Do not move product values into Engine defaults merely to avoid maintaining the project file.

## Understand precedence

For an unpackaged application, settings are applied in this order:

1. defaults declared in `Source/Common/Settings.inc`;
2. the selected `.fomain`, found from `-ApplyConfig` or by walking upward for `FO_MAIN_CONFIG`;
3. each explicitly selected `-ApplySubConfig`, in command-line order;
4. the installed-client local config stored in the writable cache, when present;
5. ordinary command-line setting overrides;
6. platform/build auto-settings.

Packaged applications use the generated internal config in place of the external
`.fomain`. Its patch area is fixed by the Engine at 20000 bytes. A packaged
build has no sub-config table and rejects `-ApplySubConfig` instead of silently
ignoring it; select the sub-config when constructing the package. Game-setting
root values also travel in metadata; the internal config carries only selected
sub-config deltas for them, including explicit false or empty overrides. The
command line still applies after local config. Later layers override earlier
scalar settings.

Metadata is applied only after `BaseEngine` construction. If project code reads
a declared game setting from `ApplicationInitHook` or an earlier startup path,
list its fully qualified name in `Baking.BootstrapGameSettings`. The config
baker then writes that setting in full to every internal config and rejects
unknown or misspelled names. Do not list ordinary runtime settings: they belong
in metadata and consuming the fixed patch area for them can make packaging fail.

Use fully qualified names such as `Server.DbStorage` in authored files and operational commands. The parser accepts short built-in names, but qualified names make ownership and review unambiguous.

## Author the root settings

Keep one deliberate value per line:

```ini
Common.GameName = My Game
Common.GameVersion = 0.1.0

Network.ServerPort = 4000
Network.WebSocketPort = 4001

Server.DbStorage = Memory
ServerNetwork.DisableNetworking = False

Baking.BakeLanguages = engl russ
Baking.BakeOutput = Baking
Baking.ServerResources = ServerResources
Baking.ClientResources = Resources
Baking.PlatformBinaries = PlatformBinaries
Baking.CacheResources = Cache

ManagedScript.Assemblies = MyGame
ManagedScript.ProjectName = MyGame
ManagedScript.TargetFramework = net10.0
ManagedScript.MsBuild = dotnet msbuild
ManagedScript.Dirs = Engine/Source/Scripting/Managed/CoreScripts Scripts
ManagedScript.Analyzers = Engine/Source/Scripting/Managed/Analyzers/FOnline.Analyzers.csproj
```

Unknown names become project custom settings and are available through `GetCustomSetting` / `FindCustomSetting`. That is intentional for game-owned configuration, but a typo in a built-in setting can therefore look valid. Add a focused project test for every content ID, port/profile, prototype name, path, or custom setting that affects startup or gameplay.

Values beginning with `+` accumulate instead of replacing. String values append with a space, vectors append elements, numeric values add, booleans use logical OR, and enums use bitwise OR; use this deliberately and test the resulting value rather than assuming list-only behavior. `$ENV{NAME}` and `$FILE{path}` resolve while the authored config is read, including during baking, so their concrete values can enter generated internal configs. `$TARGET_ENV{NAME}` and `$TARGET_FILE{path}` remain directives while baking and resolve only when a target application reads them; the current packager does not provide a general target-directive resolver. Keep credentials outside tracked config, use target forms for runtime secrets, and follow [Security and Secrets](../release/security-and-secrets.md) for command-line, logging, signing, CI, rotation, and artifact boundaries.

## Define resource packs

A resource pack selects inputs, bakers, and runtime recipients:

```ini
[ResourcePack]
Name = Protos
InputDirs = Content Maps
IncludePatterns = **
ExcludePatterns = **/Draft/**
Bakers = Proto

[ResourcePack]
Name = Maps
InputDirs = Maps
IncludePatterns = **/*.fomap
Bakers = Map

[ResourcePack]
Name = ServerScripts
InputDirs = Scripts
IncludePatterns = **/*.fos
Bakers = AngelScript
ServerOnly = True

[ResourcePack]
Name = ManagedScripts
InputDirs = Engine/Source/Scripting/Managed/CoreScripts Scripts
IncludePatterns = *
Bakers = Managed
```

Choose the backend deliberately. An AngelScript pack bakes `.fos` modules through `AngelScriptBaker`; a Managed pack compiles the configured top-level `.cs` sources and generated API into target-specific assemblies. The Managed pack must include the Engine CoreScripts and project sources selected by `ManagedScript.Dirs`; keep its assembly, analyzer, extra-source/reference, and generated-directory settings aligned with the same build. See [Managed C# Scripting](../scripting/managed-csharp.md) for the complete backend contract.

The accepted fields are:

| Field | Meaning |
|---|---|
| `Name` | Required pack identity and generated resource entry |
| `ConfigDir` | Derived owning-config directory used to resolve relative inputs; not authored in the section |
| `InputDirs` | Space-separated directories, relative to the owning config |
| `InputFiles` | Space-separated explicit files, also config-relative |
| `IncludePatterns` | Optional input glob allowlist |
| `ExcludePatterns` | Optional input glob denylist |
| `Bakers` | Space-separated baker names |
| `ServerOnly` | Emit only a server resource entry |
| `ClientOnly` | Emit only a client resource entry |
| `MapperOnly` | Emit only a mapper resource entry |

At most one side-only flag may be true. With none set, the pack is delivered to server and client. Mapper-only packs are separate. `RecursiveInput` appears in older project files but is not a current `ResourcePackInfo` field; express recursion through `IncludePatterns = **`.

Use separate packs where ownership, release cadence, side visibility, or update policy differs. Do not use pack order as a hidden gameplay override system: duplicate resource identities need an explicit project policy and a test.

## Add named sub-configs

Sub-configs are reviewed overlays for a launch mode:

```ini
[SubConfig]
Name = LocalDev
Server.DbStorage = Memory
ServerNetwork.DisableNetworking = False
Render.RenderDebug = True

[SubConfig]
Name = TutorialSmoke
Parent = LocalDev
Tutorial.Automation = True
Render.HeadlessWindow = True
Render.NullRenderer = True
Audio.DisableAudio = True
```

`Parent` names must refer to earlier sub-config sections. Multiple parents are applied left to right; later parents override earlier parents per key, then the child section wins. A launch may pass multiple `-ApplySubConfig` options, which are applied in command-line order.

Use `-ApplySubConfig NONE` for generation/baking commands that must consume only the master config. BuildTools does this for `CompileAngelScript`, `CompileManagedScripts`, `BakeResources`, and `ForceBakeResources`.

Keep sub-configs narrow:

- environment modes choose infrastructure and diagnostics;
- tests choose deterministic fixtures and headless behavior;
- scenes choose startup content;
- release modes choose product-safe settings;
- secrets stay out of sub-configs.

## Validate a configuration change

1. Reconfigure the embedding project if CMake options or the main config path changed.
2. Run `CompileAngelScript` and/or `CompileManagedScripts` for every enabled backend whose script inputs, generated API, analyzers, or metadata changed.
3. Run `BakeResources`; use `ForceBakeResources` after pack membership, baker selection, include/exclude patterns, language sets, or migration rules change.
4. Launch the smallest sub-config that consumes the changed setting.
5. Inspect startup logs for `Apply config`, `Apply sub config`, unknown/missing files, skipped languages, missing bakers, and side resource entries.
6. Run a project test that resolves custom settings and content-backed references.
7. Build/package once when internal config or runtime resource entry composition changed.

The two Engine-owned examples are executable configuration fixtures:

```bash
(cd Examples/MinimalProject && python3 validate.py)
(cd Examples/MinimalMultiplayer && python3 validate.py)
```

Use the `win64-` equivalents on Windows.

## Common failures

| Symptom | Cause | Recovery |
|---|---|---|
| `Config file not found` | Wrong working directory, `FO_MAIN_CONFIG`, or `-ApplyConfig` path | Pass the explicit config path or launch below the project root |
| `Sub config not found` | Misspelled name or section not loaded | Check section order/name and the applied master config |
| `Parent sub config not found` | Parent appears later or is absent | Move parent before child or correct the name |
| `Resource pack name not specified` | Missing `Name` | Add a unique pack name |
| Side receives an unexpected pack | Missing or incorrect side-only flag | Split packs and inspect generated resource entries |
| Incremental bake keeps old output | Pack membership or baker changed | Run `ForceBakeResources` and remove only documented disposable outputs if needed |
| Built-in setting appears ignored | Later sub-config/local config/CLI/auto layer wins | Inspect the complete precedence chain |
| Typo silently becomes a custom setting | Unknown names are project-owned by design | Add a settings/content validation test |

## Update discipline

When Engine or the embedding project is updated, inspect changes to `Settings.inc`, `Settings.cpp`, `ApplicationInit.cpp`, BuildTools project options, baking stages, and the project's `.fomain` range. Update this guide, the project config, tests, and generated references in the same change when precedence, fields, defaults, pack routing, or launch profiles change.

<a id="optional-native-components"></a>
## Optional native components

Declare the components the project uses through `SetOptionValues` before `StartProjectGeneration`. Each is a BOOL, defaults OFF and emits a numeric 0/1 compile definition. There is no aggregate component selector. Sources and Engine-owned headers stay unconditionally registered/included; optional contents are guarded internally.

```cmake
SetOptionValues(
    FO_DATABASE_SQLITE ON
    FO_DATA_SOURCE_DAT ON
    FO_IMAGE_FRM ON)
```

| Option | Owning role and dependency |
| --- | --- |
| `FO_DATA_SOURCE_DAT` | Common; DAT 2.0/2.1 and zlib |
| `FO_DATA_SOURCE_ZIP` | Common; external ZIP/BOS and bundled minizip |
| `FO_DATABASE_JSON` | Server; plain JSON, existing JSON dependency |
| `FO_DATABASE_SQLITE` | Server; SQLite and CBOR, no BSON |
| `FO_DATABASE_MONGO` | Server; mongoc and libbson |
| `FO_IMAGE_FRM` | Baker; FRM and split FRx, guarded Fallout palette |
| `FO_IMAGE_RIX`, `FO_IMAGE_ART`, `FO_IMAGE_SPR` | Baker; individual legacy importers |
| `FO_IMAGE_ZAR`, `FO_IMAGE_TIL` | Baker; independent importers and a shared guarded decoder |
| `FO_IMAGE_MOS`, `FO_IMAGE_BAM` | Baker; individual legacy importers |

Memory is mandatory in Server; PNG/TGA/FOFRM and libpng belong to Baker. Embedded/APK retain minizip with external ZIP OFF. Stock clients read baked sprites without importers or database drivers.

Retired `FO_DISABLE_MONGO`/`FO_DISABLE_SQLITE` inputs fail; clear their cache/environment and choose positive options.

<a id="baking-configuration-fingerprint"></a>
## Baking configuration fingerprint

Option declarations register their type and baking impact. `BAKING` on `DeclareBoolOption`/`DeclareValueOption`, or at the start of a plural `Declare*Options` group, includes those options in `FO_BAKING_CONFIG_HASH`. Unmarked options are excluded; the [option reference](../../reference/cmake/options.md) lists every decision.

The hash covers project identity/config selection, geometry, model/effect limits, 3D, particles, scripting backends and archive/image importers. Database providers, diagnostics, allocators, output paths and native target selection do not invalidate resources.

CMake computes SHA-256 immediately before code generation, after preceding stages and the Codegen Pre hook. It sorts registered names, normalizes BOOL to 0/1 and preserves string boundaries. New marked project options join automatically; finalize their values before `SetupCodeGeneration` consumes them.

Baker writes `Resources.baking-config-hash` and report `bakingConfigHash`. A missing/changed fingerprint clears stale outputs and forces full bake independently of `FO_BUILD_HASH`; unchanged values permit incremental bake. Both markers are removed before work and written only after successful resource baking. Reconfigure/rebuild/rebake before packaging.
