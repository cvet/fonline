---
layout: default
title: Create an FOnline Game Repository
locale: en
document_id: project-create-repository
permalink: /Docs/en/how-to/project/create-repository.html
---

# Create an FOnline Game Repository

This guide takes a game team from an empty directory to a repository that configures, bakes and passes a smoke run against a pinned Engine. It explains what each root file owns, how to add Engine as a submodule, which paths to ignore, which formats need attribute rules, and when to decide on Git LFS. It links to the owning build pages instead of repeating option lists or the settings reference.

## Source paths inspected

- `.gitattributes`
- `BuildTools/Init.cmake`
- `BuildTools/buildtools.py`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/stages/Init.cmake`
- `Docs/en/how-to/build/embedding-project.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/build/public-example-repositories.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/reference/cmake/stages.md`
- `Docs/en/tutorials/first-project.md`
- `Examples/MinimalProject`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/CMakePresets.json`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`
- `Examples/NativeExtensionSample`
- `Examples/PublicRepositoryTemplate`
- `Examples/PublicRepositories.json`

## Decide first

Some choices are cheap on day one and expensive after content exists.

| Decision | Why it comes first | Where it lives |
|---|---|---|
| Project identity: `FO_DEV_NAME`, `FO_NICE_NAME`, main `.fomain` file name | `FO_DEV_NAME` prefixes every generated target and binary (for example `<DEV>_ServerHeadless`). Presets, CI and package declarations reference these names, so change them together or not at all. | `CMakeLists.txt`, `.fomain` file name |
| Script backend | AngelScript and Managed C# have different build prerequisites, baker names and analyzers. Both can be enabled, but each enabled backend must be baked, compiled and tested. | `FO_ANGELSCRIPT_SCRIPTING`, `FO_MANAGED_SCRIPTING`; see [Managed C# Scripting](../scripting/managed-csharp.md) |
| World shape | `FO_GEOMETRY`, hex width and height and camera angle are required options. They are part of the baking fingerprint, so changing them invalidates every baked map and sprite. | `CMakeLists.txt`; see [Baking configuration fingerprint](../build/project-configuration.md#baking-configuration-fingerprint) |
| Application roles | `FO_BUILD_CLIENT`, `FO_BUILD_SERVER`, `FO_BUILD_MAPPER`, `FO_BUILD_BAKER` and `FO_BUILD_ASCOMPILER` are OFF unless something turns them on. | Presets |
| Engine location and pin | Every clone and CI runner must be able to fetch the pinned revision. | `.gitmodules`, gitlink |
| Git LFS | Moving existing binaries into LFS rewrites history. See [Decide on Git LFS](#decide-on-git-lfs). | `.gitattributes` |
| Target platforms | Build capability, package capability and support differ; check the [Support Matrix](../../reference/platforms/support-matrix.md). | Presets, CI |

## Prepare the workstation

Install Git, CMake 3.22 or newer, a real Python 3 interpreter and a C++20 toolchain. The detailed per-OS list is in [Prerequisites](../build/index.md#prerequisites); [workstation-setup.md](workstation-setup.md) covers the full workstation route.

Check the interpreter before you rely on it. `python` can be Python 2 and `python3` can be absent, and the Windows launcher `py -3` selects Python 3:

```powershell
py -3 --version
git config --global core.longpaths true
```

```bash
python3 --version
```

After Engine is added in the next section, `host-check` reports missing prerequisites without installing anything:

```powershell
py -3 Engine\BuildTools\buildtools.py host-check windows
```

```bash
python3 Engine/BuildTools/buildtools.py host-check linux
```

macOS is accepted by the command, but this guide does not describe a macOS route; see the [Support Matrix](../../reference/platforms/support-matrix.md) for what is supported.

## Create the repository and add Engine

1. Create the repository:

   ```bash
   mkdir MyGame
   cd MyGame
   git init -b main
   ```

2. Add Engine at `Engine/`. Every Engine example, every default `.fomain` pack path (`Engine/Resources/Core`) and the tutorials assume this path:

   ```bash
   git submodule add https://github.com/cvet/fonline.git Engine
   ```

3. Pin an exact revision. `git submodule add` checks out the default branch tip, which is not a release decision. Choose the revision deliberately, check it out inside the submodule and stage the new gitlink:

   ```bash
   git -C Engine checkout <engine-sha>
   git -C Engine branch -r --contains <engine-sha>
   git add .gitmodules Engine
   git submodule status Engine
   ```

   The `branch -r --contains` output must list a published branch, normally `origin/master`. A commit that exists only in your local Engine clone makes every other clone and CI runner fail when it initializes the submodule. Read `Engine/VERSION` and the [changelog](../../reference/changelog.md) for the revision you picked, and record both next to the SHA in your first commit message.

4. Initialize Engine in other clones by name. If the project has no other submodules, this is equivalent to a plain init; if it has private or large ones, it avoids fetching them:

   ```bash
   git submodule update --init --recursive Engine
   ```

   Do not use an unscoped `git clone --recurse-submodules` or `git submodule update --init --recursive` in a repository that also contains submodules you do not need. Engine itself declares no nested submodules at the inspected revision, so `--recursive` costs nothing today; keep it because a later pin may add one.

5. Never put `Engine/` in the game's `.gitignore`: Git refuses to stage an ignored path without `--force`, so a pin update would need a workaround every time. The Engine-owned examples ignore `/Engine/` only because inside the Engine repository that path is a local link, not a submodule.

6. Moving to a different Engine revision is a reviewed change, not a pointer edit. Use [Upgrade an Embedding Project](../migration/engine-upgrade.md) and avoid `git submodule update --remote`, which moves the pin to a branch tip.

### Absolute or relative submodule URL

| URL form | Use when | Constraint |
|---|---|---|
| `https://github.com/cvet/fonline.git` | The team consumes the public Engine directly. This is what the Engine examples record in `.gitmodules`. | Every runner needs outbound access to that host. |
| `../fonline.git` (relative) | The team keeps its own Engine mirror or fork on the same Git server and namespace as the game repository. | Git resolves the path against the superproject's `origin` URL. The mirror must carry the pinned commit, and every contributor and runner needs access to both repositories with the same credentials. |

If the Engine mirror is not a sibling of the game repository, use an absolute URL. Either way, keep the mirror a plain mirror of `cvet/fonline` so the pinned SHA stays identical.

## Root files and what each owns

| File or directory | Owns | Notes |
|---|---|---|
| `CMakeLists.txt` | Product identity, backend and geometry options, source registration, packages, project targets | Calls the Engine stages in fixed order. See [Write the root CMakeLists.txt](#write-the-root-cmakeliststxt). |
| `CMakePresets.json` | Per-toolchain configure and build entry points | Project-owned; Engine does not publish a preset list. See [Presets](#presets). |
| `<Name>.fomain` | Runtime and baking settings, resource packs, sub-configs | Selected by `FO_MAIN_CONFIG`. Authoring rules: [Configure a Game Project](../build/project-configuration.md). |
| `VERSION` | The project version that `Common.GameVersion` reads | Project-owned and distinct from `Engine/VERSION`. Create it with a seed value now and hand ownership to a generator; see [versioning-and-releases.md](versioning-and-releases.md). |
| `Scripts/` | AngelScript `.fos` or Managed C# `.cs` sources | Directory names are chosen by the `InputDirs` of your resource packs. |
| `SourceExt/` | Optional project-native C++ | Registered per role with `AddEngineSources`; see [Native Extensions](../native-extensions.md). |
| `Content/`, `Maps/` | Prototypes, maps and text | Names are a project convention; `MinimalMultiplayer` uses `Content`, `Maps`, `Particles`. |
| `.gitmodules`, `Engine` | Engine location and pinned gitlink | Written by `git submodule add`. |
| `.gitignore` | Generated and output trees | See [Ignore generated output](#ignore-generated-output). |
| `.gitattributes` | Line endings and binary formats | See [Declare line endings and binary formats](#declare-line-endings-and-binary-formats). |

Do not name an authored content directory `Resources`, `ServerResources`, `PlatformBinaries`, `Baking` or `Cache`. `Common.ResourcesPath` defaults to `Resources`, and the others are baking or runtime output names that the Engine examples ignore.

A typical tree after the first bring-up:

```text
MyGame/
├── Engine/                 submodule, pinned
├── .gitmodules
├── .gitignore
├── .gitattributes
├── CMakeLists.txt
├── CMakePresets.json
├── MyGame.fomain
├── VERSION
├── Scripts/
├── SourceExt/              only when the project has native code
├── Content/                prototypes and text
├── Maps/
└── Docs/                   project policy only; link to Engine docs for mechanics
```

[Embedding FOnline in a Game Project](../build/embedding-project.md) owns the ownership rule between `Engine/` and the game repository.

## Write the root CMakeLists.txt

The root file includes `Engine/BuildTools/Init.cmake`, sets the required options, then calls each stage entry point exactly once in order. Skipping a stage, repeating one or calling one out of order aborts configure. The annotated skeleton below is derived from `Examples/MinimalProject/CMakeLists.txt` and enables AngelScript, the baker and the headless server through presets:

```cmake
cmake_minimum_required(VERSION 3.22)
project(MyGame)

# Engine must be a submodule or link at Engine/ next to this file.
include(Engine/BuildTools/Init.cmake)

# Identity: DEV_NAME prefixes generated targets, NICE_NAME is the display name.
SetOption(FO_MAIN_CONFIG "MyGame.fomain")
SetOption(FO_DEV_NAME "MYGAME")
SetOption(FO_NICE_NAME "My Game")

# World shape: required, and part of the baking fingerprint.
SetOption(FO_GEOMETRY HEXAGONAL)
SetOption(FO_MAP_HEX_WIDTH 32)
SetOption(FO_MAP_HEX_HEIGHT 16)
SetOption(FO_MAP_CAMERA_ANGLE 25.6589)

# Required common output root; a per-preset build tree keeps bakes separate.
SetOption(FO_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# Backends and features.
SetOption(FO_ENABLE_3D OFF)
SetOption(FO_ANGELSCRIPT_SCRIPTING ON)
SetOption(FO_MANAGED_SCRIPTING OFF)
SetOption(FO_NATIVE_SCRIPTING OFF)
SetOption(FO_UNIT_TESTS ON)

# Stages, in the order the pipeline validates.
StartProjectGeneration()
RegisterProjectOptions()
AddThirdPartyLibraries()
# AddEngineSources(SERVER SourceExt/ServerExtension.cpp)  goes here when native code exists
RegisterEngineSources()
SetupCodeGeneration()
BuildCoreLibraries()
BuildApplications()
SetupScriptsAndBaking()
# DefinePackage and AddBakingTarget calls go here
BuildPackages()
# Project-owned custom targets such as a smoke target go here
FinalizeProjectGeneration()
```

Points that are easy to get wrong:

- `SetOption` assigns only when the variable is not already defined. The effective precedence is a matching `FO_` environment variable, then an existing cache or `-D` value, then your `SetOption`, then the declared default. Presets can therefore override these values through `cacheVariables`.
- `SetOption` and `SetOptionValues` check every `FO_` name. A retired option stops configure at the call and names its replacement; any other undeclared `FO_` name stops the Init stage with `SetOption: <name> is not a declared option` (or `SetOptionValues: ...`). Names given through `-D`, presets or the environment are not checked. Copy names from the generated [option reference](../../reference/cmake/options.md); the Managed switch is `FO_MANAGED_SCRIPTING`.
- Optional native components such as database providers are chosen with `SetOptionValues` before `StartProjectGeneration`; see [Optional native components](../build/project-configuration.md#optional-native-components).
- Native sources are added per role (`COMMON`, `SERVER`, `CLIENT`, `MAPPER`, `BAKER`, `TESTS`) with `AddEngineSources` before `RegisterEngineSources`. `Examples/NativeExtensionSample` shows `SourceExt/`, a unit-tested core library and a smoke target. Its `FO_SERVER_LIBS` link-list integration is documented in the examples as revision-pinned state, not a declared helper; re-audit it whenever the Engine pin changes.
- Packages are declared between `SetupScriptsAndBaking()` and `BuildPackages()`; a project baking target is created right after `SetupScriptsAndBaking()`. See [Packaging and Release](../release/packaging.md) and [Add a project-specific baking target](../build/embedding-project.md#add-a-project-specific-baking-target).
- The build hash is the project repository's `HEAD`. Outside a Git repository, or before the first commit, CMake falls back to a random hash without an error message (the configure log shows `Build hash: ...-random`). That is fine for a first configure, but commit before you judge incremental behavior.

The stage list, hooks and responsibilities are generated in [CMake Project Stages and Hooks](../../reference/cmake/stages.md). The meaning of generated outputs is in [Generated Content Workflow](../build/generated-content.md).

## Write the main configuration

Start the `.fomain` from `Examples/MinimalProject/FOnlineStarter.fomain` and keep one deliberate value per line. A small AngelScript configuration looks like this:

```ini
Common.GameName = My Game
Common.GameVersion = $FILE{VERSION}
Common.ResourcesPath = Resources
Common.PlatformBinariesPath = PlatformBinaries
Common.CachePath = Cache

Network.ServerPort = 4000
Network.WebSocketPort = 4001

Server.DbStorage = Memory

Baking.BakeLanguages = engl
Baking.BakeOutput = Baking

[SubConfig]
Name = Smoke
ServerNetwork.DisableNetworking = True

[ResourcePack]
Name = Metadata
InputDirs = Scripts
Bakers = Metadata
ServerOnly = True

[ResourcePack]
Name = Scripts
InputDirs = Scripts
Bakers = AngelScript
ServerOnly = True

[ResourcePack]
Name = Embedded
InputDirs = Engine/Resources/Embedded
IncludePatterns = **
Bakers = Image Effect RawCopy

[ResourcePack]
Name = Core
InputDirs = Engine/Resources/Core Engine/Resources/Embedded
IncludePatterns = **
Bakers = Image Effect RawCopy
```

Create `VERSION` with a seed such as `0.1.0` before the first bake; `$FILE{VERSION}` is read relative to the directory of this file. Resource-pack fields, sub-config inheritance, precedence and `$FILE` behavior are owned by [Configure a Game Project](../build/project-configuration.md).

Input directories of a pack are mounted recursively, and an empty `IncludePatterns` selects every file, so a pack needs no key to ask for recursion. `RecursiveInput` is not a resource-pack field and is ignored if written; do not add it.

## Presets

Keep one canonical configure preset that every contributor and every script can name without knowing the host, and add explicit presets for each toolchain you actually support. A CI job or a smoke script should call the explicit one it qualifies; a developer who wants "just build it" calls the default.

```json
{
  "version": 3,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 22,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "binaryDir": "${sourceDir}/Build/${presetName}",
      "cacheVariables": {
        "FO_BUILD_SERVER": "ON",
        "FO_BUILD_BAKER": "ON"
      }
    },
    {
      "name": "default",
      "displayName": "Default (host generator)",
      "inherits": "base"
    },
    {
      "name": "windows-vs",
      "displayName": "Windows x64 (installed Visual Studio)",
      "inherits": "base",
      "architecture": {
        "value": "x64",
        "strategy": "set"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Windows"
      }
    },
    {
      "name": "linux-gcc",
      "displayName": "Linux GCC (Ninja Multi-Config)",
      "inherits": "base",
      "generator": "Ninja Multi-Config",
      "cacheVariables": {
        "CMAKE_C_COMPILER": "gcc",
        "CMAKE_CXX_COMPILER": "g++"
      },
      "condition": {
        "type": "equals",
        "lhs": "${hostSystemName}",
        "rhs": "Linux"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "default-bake",
      "configurePreset": "default",
      "configuration": "Release",
      "targets": [
        "BakeResources"
      ]
    }
  ]
}
```

- The hidden `base` holds what every preset shares: the role switches and a per-preset `binaryDir`. Because the skeleton sets `FO_OUTPUT_PATH` to the build directory, each preset gets its own `Baking/`, `Binaries/` and `Cache/` tree, so two toolchains never share one bake.
- `default` pins no generator or compiler, so CMake chooses the host's default. The Engine examples leave generator selection on Windows to CMake for the same reason: pinning one Visual Studio version would reject newer installations.
- Pin the generator and compiler only in explicit presets. Add the clients, mapper, cross-compilation and package presets your team needs; this guide does not list them because they differ per game.
- Version 3 of the preset schema is enough for `condition` and `${hostSystemName}`, and CMake 3.21 reads it. The Engine examples declare version 3; use the lowest version your minimum CMake supports, because a newer schema makes older CMake reject the whole file.
- Add the roles you need to `base` (`FO_BUILD_CLIENT`, `FO_BUILD_MAPPER`, `FO_BUILD_ASCOMPILER`). `RunStarterSmoke`-style custom targets in the examples are guarded by `FO_BUILD_SERVER AND FO_BUILD_BAKER`.

## Ignore generated output

Ignore everything the pipeline writes and everything a tool leaves beside the sources. Anchor entries to the repository root:

```text
/Build/
/Baking/
/Baking.lock
/AS_DEBUG.lock
/Binaries/
/Cache/
/Resources/
/ServerResources/
/PlatformBinaries/
/Workspace/
/*.log
__pycache__/
```

- With `FO_OUTPUT_PATH` set to the build directory, most output lands under `Build/<preset>/`. The other entries cover runs started from the repository root, as the example `.gitignore` files do, plus the `<output>.lock` file that an active bake holds. See [Baking output ownership](../../explanation/content-pipeline/baking.md); never delete a lock file to get past a running bake.
- `Workspace/` is the conventional scratch location for task logs, dumps and screenshots.
- Add editor and IDE folders your team uses; Engine does not prescribe them.

## Declare line endings and binary formats

Byte-sensitive steps (hash-based checks, provenance records, generated-file `--check` gates, baking inputs) behave differently when checkouts on Windows and Linux disagree on line endings. Declare the policy once. This template follows the Engine's own `.gitattributes` and the policy shared by the example repositories:

```text
* text=auto eol=lf

# Windows scripts keep CRLF
*.bat  text eol=crlf
*.cmd  text eol=crlf
*.ps1  text eol=crlf

# FOnline text formats
*.fomain text
*.fopro  text
*.fomap  text
*.fofx   text
*.fofnt  text
*.fofrm  text
*.fo3d   text
*.fotxt  text
*.fos    text
*.cs     text
*.spark  text
*.efkproj text

# FOnline and common binary formats
*.spk    binary
*.efk    binary
*.png    binary
*.tga    binary
*.jpg    binary
*.wav    binary
*.ogg    binary
*.ogv    binary
*.fbx    binary
*.zip    binary
```

- `eol=lf` on `*` is the policy the example repositories use. Keep CRLF only for the Windows script types above, and for Visual Studio project files if you commit any.
- Add every authored text or binary extension your game introduces, including project-owned formats. Engine's root `.gitattributes` is a reference list for FOnline extensions, but it also contains Engine-only rules.
- Do not rely on `text=auto` to guess binaries that matter: marking a format `binary` also disables diff and merge attempts on it.
- Run `git add --renormalize .` once after adding or changing rules, and review the result before committing.

## Decide on Git LFS

Engine does not require Git LFS and its repository does not use it. A game decides up front, because converting committed binaries later rewrites history.

Decide these points before the first binary is committed:

1. Whether large binary sources (textures, audio, models, source art, packaged archives) are versioned in the game repository at all, or live in an asset store with a reference from the repository.
2. Which extensions and size threshold go to LFS, recorded in `.gitattributes` with the LFS filter.
3. Where the LFS object store lives, who can write to it, its quota and its retention. Self-hosted and hosted stores behave differently; the choice is project policy.
4. How CI obtains objects. A checkout that skips LFS produces pointer files, which then fail baking in confusing ways. Fetch LFS objects in the same job step as the checkout and fail early if a pointer file reaches the bake. [continuous-integration.md](continuous-integration.md) covers job ordering.
5. How you audit that every file matching an LFS rule is actually stored through LFS.

Without LFS, keep binaries small and avoid committing generated outputs. Either way, put the decision in the project documentation.

## Copy an example or start from the skeleton

| Starting point | What it is | Use it for |
|---|---|---|
| `Examples/MinimalProject` | An executable starter: headless server, baker, one script, one native server extension, one smoke runner and Windows/Linux presets | The fastest first green. Copy it, then trim. |
| `Examples/MinimalMultiplayer` | A small client/server project with content, a package and a multiplayer smoke | A reference for client roles, content packs and a package declaration |
| `Examples/NativeExtensionSample` | The advanced native hook/export/test path with `SourceExt/` and `Tests/` | A reference when the project has native code |
| `Examples/PublicRepositoryTemplate` | The publication overlay for public Engine example repositories: placeholders, governance files, workflows and repository metadata | Not a game starter. Do not copy it into a game; its files expect `example-repository.json` and an Engine-owned staging process |

The route that gets a new team green quickest:

1. Copy the contents of `Examples/MinimalProject` into the new repository root, excluding any local `Engine` link and any `Build/` directory. Remove the `/Engine/` line from the copied `.gitignore`: the example ignores that path because inside the Engine repository it is a local link, but a game repository tracks it as a submodule.
2. Add Engine as the submodule described above.
3. Change the identity as one operation: `FO_DEV_NAME`, `FO_NICE_NAME`, the `.fomain` file name, the `FO_MAIN_CONFIG` value, and the names in the presets and the smoke target. Do not rename identifiers one at a time.
4. Run the smoke below and require green before editing any gameplay.
5. Only then trim the demonstration pieces (the native extension and its `INTERFACE` library, the demonstration remote calls) in a separate change, keeping the script start marker and the smoke runner contract.

The examples are written to find Engine at `Engine/` beside `CMakeLists.txt`. To run one in place inside the Engine repository for evaluation, create a link there and remove it afterwards. The example's own `.gitignore` excludes the link; never commit it. Remove a Windows junction with `cmd /c rmdir Engine`, and a Linux symlink with `rm Engine`; a recursive delete of the link can follow it into the Engine checkout.

```powershell
cd Examples\MinimalProject
New-Item -ItemType Junction -Path Engine -Target ..\..
```

```bash
cd Examples/MinimalProject
ln -s ../.. Engine
```

Without the link, configure stops with `include could not find requested file: Engine/BuildTools/Init.cmake`. See [First FOnline Headless Project](../../tutorials/first-project.md).

## First-run verification

On a Linux host, install the prerequisites owned by the pinned Engine revision once:

```bash
Engine/BuildTools/prepare-workspace.sh linux-packages
```

With a project copied from `Examples/MinimalProject` and its presets (`windows`, `linux`, `windows-smoke`, `linux-smoke`), run from the repository root:

```powershell
cmake --preset windows
cmake --build --preset windows-smoke
```

```bash
cmake --preset linux
cmake --build --preset linux-smoke
```

The smoke target builds the headless server, bakes resources and runs the `StarterSmoke` sub-config. Success requires every marker and exit code zero:

```text
starter_native_extension_value=42
starter_server_started
starter_smoke_passed
```

For a project that starts from the skeleton in this guide instead, the equivalent checks are:

```powershell
cmake --preset default
cmake --build --preset default-bake
```

```bash
cmake --preset default
cmake --build --preset default-bake
```

Then build the generated `<FO_DEV_NAME>_ServerHeadless` target, run it with `-ApplyConfig <project-root>/MyGame.fomain -ApplySubConfig Smoke`, and write a smoke runner that, like `run_starter_smoke.py`, enforces a timeout, requires lifecycle markers from your start event and fails when a marker is missing. A process that merely starts and exits is not a passing smoke. The default preset's bake and a successful server start are the two facts to establish before adding anything else. The skeleton's configure was observed to pass option validation and reach third-party registration on Windows with an installed Visual Studio generator; it was not run to completion or built as part of writing this guide, so treat your first run as the check.

Rebuild with `ForceBakeResources` after changing pack membership, bakers or languages. If a build looks stale, see [Generated Content Workflow](../build/generated-content.md#recovery).

## Troubleshooting the first run

| Symptom | Cause | Recovery |
|---|---|---|
| `include could not find requested file: Engine/BuildTools/Init.cmake` followed by `Unknown CMake command "SetOption"` | `Engine/` is missing, uninitialized or is a link to the wrong directory | Run `git submodule update --init --recursive Engine`, or fix the link target |
| `git submodule update` cannot find the pinned commit | The SHA exists only in a local clone, or the mirror lacks it | Pin a commit that `git -C Engine branch -r --contains <sha>` lists, or update the mirror |
| Configure aborts naming an empty required variable | A required option from the table in [Write the root CMakeLists.txt](#write-the-root-cmakeliststxt) is missing | Add the `SetOption` line |
| `SetOption: <name> is not a declared option` or `<name> was removed` | The name passed to `SetOption` or `SetOptionValues` is misspelled or retired | Compare it with the [option reference](../../reference/cmake/options.md); for a retired name follow the replacement in the message |
| An option appears to have no effect | The name came from `-D`, a preset or the environment, which are not checked, and is misspelled or retired | Compare it with the [option reference](../../reference/cmake/options.md) |
| Bake fails on pointer files or unexpected text in binaries | LFS objects were not fetched | Fetch LFS objects before baking and check the `.gitattributes` filters |
| Mass line-ending changes in a diff | Missing or late `.gitattributes` | Add the rules, run `git add --renormalize .`, review |

## Validation checklist

- [ ] `git submodule status Engine` shows the intended SHA without a `+` or `-` prefix, and that SHA is listed by `git -C Engine branch -r --contains <sha>` as published.
- [ ] A fresh clone followed by `git submodule update --init --recursive Engine` reproduces the checkout, including on the CI runner image.
- [ ] `Engine/` is not ignored, and `.gitmodules` records the URL form you chose.
- [ ] `CMakeLists.txt` sets every required option, uses only declared option names, and calls all ten stages once in order.
- [ ] `FO_DEV_NAME`, `FO_NICE_NAME`, the `.fomain` name and the presets agree.
- [ ] `VERSION` exists and `Common.GameVersion` resolves from it.
- [ ] `.gitignore` covers build, bake, lock, log and workspace output and no authored directory.
- [ ] `.gitattributes` declares the line-ending policy and every binary format, and the LFS decision is recorded.
- [ ] The default preset configures, and the smoke or bake target is green on every host you claim.
- [ ] The smoke requires lifecycle markers and a zero exit code, not merely a started process.
- [ ] A second clone passes the same commands without local state.
