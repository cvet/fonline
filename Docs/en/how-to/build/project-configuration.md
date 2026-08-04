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

Packaged applications use the generated internal config in place of the external `.fomain`. The command line still applies after local config. Later layers override earlier scalar settings.

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
```

Unknown names become project custom settings and are available through `GetCustomSetting` / `FindCustomSetting`. That is intentional for game-owned configuration, but a typo in a built-in setting can therefore look valid. Add a focused project test for every content ID, port/profile, prototype name, path, or custom setting that affects startup or gameplay.

Values beginning with `+` accumulate instead of replacing. String values append with a space, vectors append elements, numeric values add, booleans use logical OR, and enums use bitwise OR; use this deliberately and test the resulting value rather than assuming list-only behavior. `$ENV{NAME}` and `$FILE{path}` resolve while the authored config is read, including during baking, so their concrete values can enter generated internal configs. `$TARGET_ENV{NAME}` and `$TARGET_FILE{path}` remain directives while baking and resolve on the target application or packaging host. Keep credentials outside tracked config, use target forms for sensitive values, and follow [Security and Secrets](../release/security-and-secrets.md) for command-line, logging, signing, CI, rotation, and artifact boundaries.

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
```

The accepted fields are:

| Field | Meaning |
|---|---|
| `Name` | Required pack identity and generated resource entry |
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

Use `-ApplySubConfig NONE` for generation/baking commands that must consume only the master config. BuildTools does this for `CompileAngelScript`, `BakeResources`, and `ForceBakeResources`.

Keep sub-configs narrow:

- environment modes choose infrastructure and diagnostics;
- tests choose deterministic fixtures and headless behavior;
- scenes choose startup content;
- release modes choose product-safe settings;
- secrets stay out of sub-configs.

## Validate a configuration change

1. Reconfigure the embedding project if CMake options or the main config path changed.
2. Run `CompileAngelScript` when script inputs or metadata changed.
3. Run `BakeResources`; use `ForceBakeResources` after pack membership, baker selection, include/exclude patterns, language sets, or migration rules change.
4. Launch the smallest sub-config that consumes the changed setting.
5. Inspect startup logs for `Apply config`, `Apply sub config`, unknown/missing files, skipped languages, missing bakers, and side resource entries.
6. Run a project test that resolves custom settings and content-backed references.
7. Build/package once when internal config or runtime resource entry composition changed.

The two Engine-owned examples are executable configuration fixtures:

```bash
python BuildTools/buildtools.py validate linux-starter-smoke
python BuildTools/buildtools.py validate linux-tutorial-smoke
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
