---
layout: default
title: Configure a Game Project
locale: en
document_id: project-configuration
permalink: /Docs/en/how-to/build/project-configuration.html
---

# Configure a Game Project

Author `.fomain`, resource packs and sub-configs here. [Configuration and Data Sources](../../reference/settings/configuration-and-data-sources.md) owns runtime rules; [generated settings](../../../generated/api/settings.md) lists built-in names.

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

Start with [MinimalProject](../../../../Examples/MinimalProject/README.md) (headless) or [MinimalMultiplayer](../../../../Examples/MinimalMultiplayer/README.md) (client/server). Keep the game as CMake root and select its master config:

```cmake
include(Engine/BuildTools/Init.cmake)

SetOption(FO_MAIN_CONFIG "MyGame.fomain")
SetOption(FO_DEV_NAME "MYGAME")
SetOption(FO_NICE_NAME "My Game")
```

`FO_MAIN_CONFIG` selects the project config at configure time; `.fomain` owns runtime/baking settings. Keep product values in the project, not Engine defaults.

## Understand precedence

Unpackaged settings apply in this order:

1. defaults declared in `Source/Common/Settings.inc`;
2. the selected `.fomain`, found from `-ApplyConfig` or by walking upward for `FO_MAIN_CONFIG`;
3. each explicitly selected `-ApplySubConfig`, in command-line order;
4. the installed-client local config stored in the writable cache, when present;
5. ordinary command-line setting overrides;
6. platform/build auto-settings.

Packaged applications replace `.fomain` with generated internal config and a fixed 20000-byte patch area. They have no sub-config table and reject `-ApplySubConfig`; choose it when packaging. Root game-setting values travel in metadata; internal config holds selected sub-config deltas, including false/empty overrides. Command-line overrides follow local config; later layers replace earlier scalar values.

Metadata loads after `BaseEngine` construction. List fully qualified game settings read by `ApplicationInitHook` or earlier startup code in `Baking.BootstrapGameSettings`; config Baker writes their full values into every internal config and rejects unknown/misspelled names. Keep ordinary runtime settings in metadata; exhausting the fixed patch area can break packaging.

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

On Engine/project updates, inspect `Settings.inc`, `Settings.cpp`, `ApplicationInit.cpp`, BuildTools options/baking stages and project `.fomain`. Reconcile this guide, project config, tests and generated references in the same change when precedence, fields/defaults, pack routing or launch profiles change.

## Optional native components

Select components with `SetOptionValues` before `StartProjectGeneration`: BOOL, OFF by default, numeric 0/1 definitions, no aggregate selector. Register/include all Engine sources/headers unconditionally; files guard optional contents.

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
| `FO_AUDIO_ACM` | Baker; Interplay ACM decoder, built-in Ogg Vorbis encoder |

Server requires Memory; Baker requires PNG/TGA/FOFRM and libpng, plus WAV/Ogg and Vorbis/Ogg for audio. Embedded/APK keep minizip with external ZIP OFF. Clients read baked sprites and Vorbis audio without source importers/database drivers.

Retired `FO_DISABLE_MONGO`/`FO_DISABLE_SQLITE` inputs fail; clear their cache/environment and choose positive options.

## Baking configuration fingerprint

Declarations register type and baking impact. Mark `DeclareBoolOption`/`DeclareValueOption`, or a leading plural `Declare*Options` argument, with `BAKING` for inclusion in `FO_BAKING_CONFIG_HASH`. Unmarked options are excluded; [option reference](../../reference/cmake/options.md) lists each decision.

Hash inputs: project identity/config, geometry, model/effect limits, 3D, particles, scripting and archive/image/audio importers. Database providers, diagnostics, allocators, output paths and native targets are excluded.

CMake computes SHA-256 just before codegen, after preceding stages and Codegen Pre. Registered names are sorted, BOOLs become 0/1 and string boundaries remain distinct. Marked project options join automatically; finalize values before `SetupCodeGeneration`.

Successful resource baking writes `Resources.baking-config-hash` and report `bakingConfigHash`. Missing/changed fingerprints clear stale outputs and force full bake independently of `FO_BUILD_HASH`; unchanged fingerprints allow incremental bake. Both markers are removed before work and written only on success. Reconfigure/rebuild/rebake before packaging.
