---
layout: default
title: Troubleshoot Project Setup
locale: en
document_id: setup-troubleshooting
permalink: /Docs/en/troubleshooting/setup.html
---

# Troubleshoot Project Setup

This page maps the failures a new game project meets between a fresh machine and its first green build to their causes and fixes. Each entry quotes the message exactly as BuildTools, the CMake pipeline or the runtime produces it, so search this page for the text you see. Runtime and debugger failures after a successful build belong to [Native, AngelScript, and Managed C# Debugging](debugging.md).

Messages that come from Engine source are quoted from it. Messages owned by CMake were captured with CMake 4.1.1 on Windows x64 and can differ in wording or line wrapping on other versions; CMake wraps long messages at 80 columns and treats a semicolon inside a message as a list break. Messages owned by Git, the compiler or the operating system are given in their usual wording and were not reproduced; the entry says so.

## Source paths inspected

- `BuildTools/Init.cmake`
- `BuildTools/buildtools.py`
- `BuildTools/package.py`
- `BuildTools/build.cmd`
- `BuildTools/prepare-workspace.sh`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/Options.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/ProjectOptions.cmake`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Source/Tools/Baker.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/UpdaterBackend.cpp`
- `Source/Client/Updater.cpp`
- `Source/Common/MetadataRegistration.cpp`
- `Docs/contract-change-dispositions.json`
- `Docs/en/tutorials/first-project.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/explanation/content-pipeline/baking.md`
- `Docs/en/reference/metadata/index.md`
- `Examples/MinimalProject/CMakeLists.txt`
- `Examples/MinimalProject/CMakePresets.json`

## Triage

1. Copy the first error, not the last. CMake prints `Configuring incomplete, errors occurred!` after every failure, and later errors often follow from the first one.
2. Identify the layer from the message prefix. `[BuildTools]` lines and bare strings come from `buildtools.py` or the wrapper scripts. `CMake Error` blocks come from CMake or the Engine pipeline. Text from a running server, client or baker is a bake or runtime failure.
3. Print the paths BuildTools resolved. A wrong working directory explains many "not found" and "not ready" results:

```powershell
py -3 Engine\BuildTools\buildtools.py env --summary-only
```

```bash
python3 Engine/BuildTools/buildtools.py env --summary-only
```

4. Fix the failing owner. Do not delete the workspace, the build tree or a lock file as a first step; the entries below say when a clean directory is the right fix.

## Toolchain and host tools

Install and verify the tools first with [Prepare a Workstation for an FOnline Project](../how-to/project/workstation-setup.md).

### Python is missing or is Python 2

```text
Python not found
```

The Engine wrapper scripts print this when they find neither `py` nor `python` (Windows) or neither `python3` nor `python` (POSIX) on `PATH`. Install Python 3.12 or newer. On Windows, install the `py` launcher with it and open a new shell.

```text
  File "Engine/BuildTools/buildtools.py", line <n>
    platform: str
            ^
SyntaxError: invalid syntax
```

`python` resolved to Python 2, which cannot parse the type annotations in `buildtools.py`. Run the same command with `py -3` (Windows) or `python3` (elsewhere). If `python3` is also missing or old, fix `PATH` order instead of editing scripts. An older Python 3 fails inside BuildTools instead: BuildTools imports `NotRequired` from `typing` (3.11 and newer) and passes `onexc=` to `shutil.rmtree` (3.12 and newer). That failure was derived from the source and not reproduced on an old interpreter.

```text
Could NOT find Python3 (missing: Python3_EXECUTABLE Interpreter)
```

CMake runs `find_package(Python3 REQUIRED COMPONENTS Interpreter)` during the Codegen stage and needs a Python 3 interpreter even when your own scripts use another one. Make a Python 3 discoverable through `PATH` (or the Windows launcher registration), then configure again. The Managed C# runtime step additionally requires Python 3.11 or newer.

### CMake is too old or cannot read the presets

```text
CMake Error at CMakeLists.txt:1 (cmake_minimum_required):
  CMake 3.22 or higher is required.  You are running version <installed>
```

The Engine entry point asks for CMake 3.22. Install a newer CMake and delete the build directory that the older one created.

```text
CMake Error: Could not read presets from <project-root>:
CMakePresets.json:<line>: Unrecognized "version" <n>: must be >=1 and <=<supported>
```

The preset file uses a schema newer than the installed CMake understands. The Engine examples declare `"version": 3`, which every supported CMake (3.22 or newer) reads. This entry was reproduced with a larger version number than any real example uses; the supported maximum in the message depends on the installed CMake. Install a newer CMake, or lower the schema version of your own file after removing features it uses.

```text
CMake Error: No such preset in <project-root>: "<name>"
```

The preset name is misspelled or `cmake` ran from a directory that does not contain the `CMakePresets.json` you meant. Run `cmake --list-presets` from the project root.

### The compiler or Visual Studio is missing or too old

```text
Please install Visual Studio with C++ Build Tools or Build Tools for Visual Studio
```

`buildtools.py host-check windows` (and `prepare-host-workspace windows`) found no installation that includes the component `Microsoft.VisualStudio.Component.VC.Tools.x86.x64`. Install Visual Studio 2022 or Build Tools with the "Desktop development with C++" workload.

```text
Generator

  Visual Studio 17 2022

could not find any instance of Visual Studio.
```

CMake was asked for a Visual Studio generator that is not installed. Install that version, or select the generator that exists on the machine. The first lines of a CMake error name the generator; the Engine example presets leave the generator unset on Windows so CMake picks the newest installed one.

```text
MSVC <MSVC_VERSION> is below the required minimum 1944 (toolset 14.44 / Visual Studio 2022 17.14).
Clang <version> is below the required minimum 20.0
GCC <version> is below the required minimum 13.0
Unsupported compiler '<id>'. Supported: Clang >= 20, GCC >= 13, MSVC >= 2022, AppleClang (any).
```

The Init stage rejects a compiler below the minimum right after configure detects it. Update Visual Studio through its installer, or install GCC 13 or Clang 20 and select it with the `CC` and `CXX` environment variables or the `CMAKE_C_COMPILER` and `CMAKE_CXX_COMPILER` cache entries of your preset. CMake remembers the compiler in the build directory, so configure into a fresh directory after changing it. On Debian or Ubuntu, `prepare-host-workspace linux common-packages` installs Clang 20. The `-win7` build platforms additionally pin toolset 14.44; see [Build Workflow](../how-to/build/index.md#windows-7-compatibility-lane).

### Linux packages cannot be installed

```text
Please use a Debian/Ubuntu host with apt-get available
Please install sudo or run the preparation script as root
```

The `*-packages` features install through `apt-get` and need root or `sudo`. On another distribution, install the same tools with its package manager and prepare only the workspace parts (`toolset`, `web` and so on), which do not install packages.

```text
Workspace part common-packages is not ready (<version>)
```

This appears with `--check` (or the `check` wrapper word) before the packages were installed. Run the same command without `--check`.

### Ninja or llvm-nm cannot be found

```text
ninja not found; install Ninja or Visual Studio CMake tools
CMake Error: CMake was unable to find a build program corresponding to "<generator>".  CMAKE_MAKE_PROGRAM is not set.  You probably need to select a different build tool.
```

A preset or a Web or Android build selected a Ninja or Makefile generator whose program is not on `PATH`. On Windows, add the Visual Studio "C++ CMake tools for Windows" component, which BuildTools searches for `ninja.exe`, or install Ninja. The Linux preset of `Examples/MinimalProject` uses `Ninja Multi-Config`.

```text
Symbol reader for the Managed interop shim table not found. Install LLVM (llvm-nm) or the Visual Studio "C++ Clang tools for Windows" component, or set FO_MANAGED_NM
Managed runtime for Windows cannot be built on a non-Windows host (dotnet/runtime has no such cross-target); build it on Windows and point FO_MANAGED_RUNTIME_PREBUILT at the published output/mono/<triplet> tree
```

Both belong to Managed C# configurations. The first needs an `llvm-nm` or `nm` that CMake can find; set `FO_MANAGED_NM` to its full path if it is installed elsewhere. The second states the supported route: build the runtime tree on Windows and hand it over through the environment variable.

## Repository and Engine checkout

### The Engine include cannot be found

```text
CMake Error at CMakeLists.txt:3 (include):
  include could not find requested file:

    Engine/BuildTools/Init.cmake


CMake Error at CMakeLists.txt:4 (SetOption):
  Unknown CMake command "SetOption".
```

The second error is a consequence of the first: `SetOption` is defined by `Init.cmake`, so nothing after the failed include can run. The include path is relative to the source directory that holds `CMakeLists.txt`. Check these causes in order:

1. The submodule was never initialized. `git submodule status Engine` shows a line that starts with `-`, and `Engine/` is empty.
2. A new clone, or a checkout made with `git worktree add`, copies the project without populating its submodules. Initialize it as shown below.
3. The project is an Engine-owned example run in place, which expects an `Engine` entry beside its `CMakeLists.txt`. Create the junction or link described in the [first project tutorial](../tutorials/first-project.md).
4. `cmake` ran from, or was pointed at, a directory other than the project root.
5. The directory exists under another spelling, such as `engine`, on a case-sensitive filesystem.

```bash
git submodule status Engine
git submodule update --init Engine
ls Engine/BuildTools/Init.cmake
```

```powershell
git submodule status Engine
git submodule update --init Engine
Test-Path Engine\BuildTools\Init.cmake
```

### Submodule scope and state

Name the path when you initialize or update: `git submodule update --init Engine`. A bare `git submodule update --init --recursive`, or `git clone --recurse-submodules`, also initializes every other submodule of the project, including large content repositories that the project may want to keep out of a developer or CI checkout. At this Engine revision `Engine` declares no submodules of its own, so naming the path is sufficient and `--recursive Engine` is equivalent.

After `git pull` or a branch switch in the project, the `Engine/` checkout stays at its previous commit until you run `git submodule update Engine`. A `+` at the start of the `git submodule status Engine` line means the checkout differs from the commit the project records. Do not treat a detached HEAD inside `Engine/` as a problem; it is the normal state. [Upgrade an Embedding Project](../how-to/migration/engine-upgrade.md#record-the-engine-identities) explains the three Engine identities and how to compare them.

### Checkout case drift on Windows

```text
[BuildTools] Checkout case drift: <path> -> <tracked-name>
Checkout case drifted from the git index in <n> entries
```

A case-only rename is recorded in the Git index, but on a case-insensitive filesystem an existing directory keeps its old spelling. Resource and include lookups are case-sensitive, so the symptom appears later and far away: a file that is plainly on disk is reported as missing while baking, or a Linux CI job fails on a path that works on the Windows machine. A fresh clone does not reproduce it.

Run the repair from the project root. `--check` reports drift and exits non-zero without renaming anything:

```powershell
py -3 Engine\BuildTools\buildtools.py repair-checkout-case --check
py -3 Engine\BuildTools\buildtools.py repair-checkout-case
```

```bash
python3 Engine/BuildTools/buildtools.py repair-checkout-case --check
python3 Engine/BuildTools/buildtools.py repair-checkout-case
```

A successful run logs `Checkout entry names match the git index`. The command recurses into submodules.

```text
[BuildTools] Tracked path is checked in under more than one spelling: <a> and <b>
Case-conflicting tracked paths have to be fixed in the repository
```

The index holds the same path under two spellings. They are two paths on Linux and one entry on Windows, so the command refuses to guess. Decide which spelling is correct, remove the other with `git rm --cached`, commit, and run the repair again.

### Paths are too long

```text
C1083: Cannot open compiler generated file
error: unable to create file <path>: Filename too long
```

The first line is a compiler failure at the Windows path limit; a note in `BuildTools/cmake/stages/ThirdParty.cmake` records it at 297 characters while building the managed runtime. The second is Git's usual wording when `core.longpaths` is off and was not reproduced. Fix them in this order:

1. Move the checkout to a short root such as `C:\fo\game`. Everything below it, including `Build/`, adds length.
2. Run `git config --global core.longpaths true` before cloning.
3. Enable the operating-system long path setting (`LongPathsEnabled` or the "Enable Win32 long paths" policy). Engine does not set it.
4. For Managed C# on Windows, set `FO_DOTNET_DIR` to a short directory before configuring; it replaces the default `dotnet` directory inside the build tree.

Details and the reasons Engine only partly covers this are in [Prepare a Workstation for an FOnline Project](../how-to/project/workstation-setup.md#windows-long-paths-and-links).

### Symbolic links or junctions cannot be created

`buildtools.py validate` creates an `Engine` link, the Engine-owned examples need one when they run in place, and some projects track editor-extension links. On Windows, creating a symbolic link with `mklink /d` needs an elevated shell or Developer Mode; the usual cmd message is `You do not have sufficient privilege to perform this operation.` (not reproduced on the Windows host used for this page, which already had link permission). Use one of these fixes:

- Enable Developer Mode, or run the shell elevated for the one command.
- Create a junction instead of a symbolic link for a directory. It needs no privilege: `New-Item -ItemType Junction -Path Engine -Target <engine-checkout>`.
- For links that the project tracks, set `git config core.symlinks true` before the checkout. With the setting off, Git writes a symbolic link as a small text file holding the target path, and the tool that expects a directory fails with a "not found" error. Fix the setting and check the repository out again.

## Workspace and bake state

### A workspace part is not ready

```text
Workspace part <name> is not ready (<version>)
Workspace EMSDK is not prepared: <path>
Toolset build directory not found: <path>
FO_XWIN_ROOT is not set. Run prepare-workspace.sh windows-cross to splat the Windows SDK.
```

Prepared parts record `<part>-version.txt` in `FO_WORKSPACE`. The first message means the marker is absent or holds a different version: the part was never prepared here, an Engine update changed its pin, or the command ran from a different directory and therefore a different `Workspace`. The other three say a build needs a part that was not prepared in the workspace BuildTools resolved.

1. Print the resolved paths with `env --summary-only` and confirm `FO_WORKSPACE`.
2. Run the same `prepare-host-workspace` or `prepare-workspace` command without `--check`.
3. Do not create or edit `*-version.txt` files by hand; they assert that the directory contents match.
4. For the Windows SDK on Linux, use the named feature: `prepare-host-workspace linux windows-cross`.

### A stale build directory rejects the toolchain

```text
CMake Error: Error: generator : Ninja Multi-Config
Does not match the generator used previously: Visual Studio 17 2022
Either remove the CMakeCache.txt file and CMakeFiles directory or choose a different binary directory.
```

A build directory is bound to its first generator and compiler. After you change a preset, install a new compiler, move the project or switch generators, configure into a new directory. If you keep the directory, remove only its `CMakeCache.txt` and `CMakeFiles` as the message says. Do not delete the baking output or the workspace for this error.

### A bake or debug lock is held

```text
Resource baking output lock is unavailable
AngelScript debug output lock is unavailable
```

Another process owns the sibling `<output-directory>.lock` file (or `AS_DEBUG.lock` in a debug-instrumented AngelScript build). The failed baker leaves the previous reports, hash and resources unchanged. Find the owner by executable path and command line, wait for it to exit and retry. Do not delete a lock file: an existing file does not mean a bake is running, and removing a held POSIX lock lets a second process lock a new file of the same name. Do not start a bake while a game process still reads the same output. [Baking Pipeline](../explanation/content-pipeline/baking.md#output-ownership-and-competing-processes) defines the ownership rules.

### Server and client resources come from different bakes

```text
Connected client <host> runs metadata version <a> while the server runs <b> - its synced resources do not match this server
Distributed client resources were baked apart from the server resources
Client updater: synced resources run metadata version <a> while the server runs <b>, resources <directory>
Baked metadata does not start with the metadata file marker
Baked metadata file version does not match the engine, resources must be rebaked
Baked metadata carries no version, resources must be rebaked
Build hash file <path> has wrong hash
```

A server and every client it serves must run resources and binaries from one bake. Entity payloads address properties by their order in the baked metadata, so mixed sets are rejected on purpose and not treated as a compatibility mode. The last message comes from the packager and means a built output belongs to a different commit or configure than the one being packaged.

1. Rebuild and rebake from a single clean state: reconfigure, rebuild the targets, run `ForceBakeResources`.
2. Deploy the server packs at `Common.ResourcesPath` and the client distribution in its `Client` child together.
3. After switching commits, reconfigure and rebuild before baking again. `FO_BUILD_HASH` is compiled into the binaries and written to the bake markers, so an older binary cannot complete a newer bake.
4. Keep `Network.ForceMetadataVersion` for simulating a stale client in tests; it is not a fix.

The metadata format and the four layers that enforce it are in [Generated API and Metadata](../reference/metadata/index.md).

### Downloads fail

```text
Download <label> failed (<reason>), attempt <n>/5, retry in <seconds>s
received <written> of <declared> bytes
```

Workspace preparation downloads archives from the vendors' hosts and makes up to five attempts. `received <written> of <declared> bytes` means a connection ended early; BuildTools compares the byte count with the upstream `Content-Length` so a truncated archive does not unpack into a distant failure. Retry from a stable network, or put a pull-through mirror in front of the downloads with `FO_DOWNLOAD_MIRROR`, a prepared-workspace cache with `FO_WORKSPACE_CACHE`, and `FO_CI_CA` for a machine whose root store cannot be repaired. The variables and their rules are in [Build Workflow](../how-to/build/index.md#fetching-through-a-mirror-of-your-own). The mirror does not cover `git clone` of the Emscripten SDK and the .NET runtime source, which contact GitHub directly.

## Project options

### A misspelled or retired option stops configure

`SetOption(<name> <value>)` sets a variable when it is not defined yet; `SetOptionValues` stores the value in the CMake cache. Both check every name that starts with `FO_`, because that prefix belongs to the Engine. A retired name stops configure at the call itself, and the message names the replacement. The retired Mono backend flag is the typical example:

```cmake
SetOption(FO_MONO_SCRIPTING ON)
```

```text
SetOption: FO_MONO_SCRIPTING was removed. Use FO_MANAGED_SCRIPTING for the Managed C# backend
```

`FO_MONO_SCRIPTING` is deliberately not an alias: replace the line with `FO_MANAGED_SCRIPTING` and choose `ON` or `OFF` on purpose. `FO_BUILD_EDITOR`, `FO_INTERNAL_CONFIG_CAPACITY` and `FO_DISABLE_UNQLITE` are refused the same way, each with its own guidance.

Any other `FO_` name that the pipeline does not declare stops configure in the Init stage, as soon as the options are declared and before the compiler and platform checks. A name set later, for example in a stage hook, fails at its call. The message starts with the command that received the name:

```text
SetOptionValues: FO_MANAGED_SCRIPTNG is not a declared option. Check the name against the CMake option reference; project variables need a prefix other than FO_
```

Compare the spelling with [CMake options](../reference/cmake/options.md). Names without the `FO_` prefix are not checked, so project variables such as `MYGAME_FEATURE` keep working.

The check covers only names passed to `SetOption` and `SetOptionValues`. A name passed with `-D`, in a preset's `cacheVariables` or as an environment variable is not checked, and an undeclared one has no effect. The visible effects depend on the option: a backend target such as `CompileManagedScripts` does not exist, because it is created only when `FO_MANAGED_SCRIPTING` is enabled, or a feature stays at its default `OFF`. Detect it from the configure log. Each declared option is printed once as `-- <name> = <value>`:

```text
-- FO_ANGELSCRIPT_SCRIPTING = OFF
-- FO_MANAGED_SCRIPTING = OFF
```

An option you set that never appears in this list is not declared. For a name passed with `-D` or in a preset's `cacheVariables`, CMake may also end the configure with `Manually-specified variables were not used by the project`. Compare your files with the declared names in [CMake options](../reference/cmake/options.md), or run this review script from the project root. Save it outside the repository, for example as `check_options.py`:

```python
import json
import re
import sys
from pathlib import Path

model = json.loads(Path("Engine/BuildTools/cmake/ProjectInterface.json").read_text(encoding="utf-8"))
declared = {option["name"] for option in model["options"]}
pattern = re.compile(r"\bFO_[A-Z][A-Z0-9_]*\b")

for name in sys.argv[1:]:
    text = Path(name).read_text(encoding="utf-8")
    for token in sorted(set(pattern.findall(text)) - declared):
        print(f"{name}: {token} is not a declared option")
```

```powershell
py -3 check_options.py CMakeLists.txt CMakePresets.json
```

```bash
python3 check_options.py CMakeLists.txt CMakePresets.json
```

The output is a review list, not a verdict. Variables that are not options, such as `FO_SERVER_LIBS` or `FO_ENGINE_ROOT`, are listed too; every other name is a typo or a retired option.

Two retired inputs fail even outside these commands. If the `FO_DISABLE_MONGO` or `FO_DISABLE_SQLITE` variable is defined (cache, environment or a `SetOption`/`SetOptionValues` call), configure stops with the text below, after which you choose `FO_DATABASE_MONGO` or `FO_DATABASE_SQLITE` through `SetOptionValues` and clear the stale cache entry:

```text
FO_DISABLE_MONGO was removed. Select positive FO_DATABASE_MONGO/FO_DATABASE_SQLITE options with SetOptionValues; clear the retired cache entry
```

### A required option is missing

```text
FO_MAIN_CONFIG not specified
```

The Init stage requires `FO_MAIN_CONFIG`, `FO_DEV_NAME`, `FO_NICE_NAME`, `FO_GEOMETRY`, `FO_MAP_HEX_WIDTH`, `FO_MAP_HEX_HEIGHT`, `FO_MAP_CAMERA_ANGLE` and `FO_OUTPUT_PATH`, and reports the first one that is empty with the text `<name> not specified`. The usual cause is order: `SetOption` calls placed after `StartProjectGeneration()` run too late, because the stage checks the values while it runs. Move every project option before the first stage call, or provide it through a `-D` entry, a preset or an `FO_` environment variable.

Related checks in the same stages:

```text
FO_MAP_MAX_SIZE must be an integer from 200 to <limit> (<reason>); got '<value>'
FO_UNITY_BUILD_BATCH_SIZE must be a positive integer; got '<value>'
AngelScript compiler build can not be without AngelScript scripting enabled
Code coverage build can not be mixed with other builds
Invalid requested configuration type
```

The first two reject an out-of-range value, the third a `FO_BUILD_ASCOMPILER` build without `FO_ANGELSCRIPT_SCRIPTING`, the fourth a coverage build combined with another build option, and the last a `CMAKE_BUILD_TYPE` that the project does not list. [Configure a Game Project](../how-to/build/project-configuration.md) owns the option set and the configuration rules.

### Stages run out of order or an override is invalid

```text
Pipeline: stage 'ProjectOptions' invoked before 'Init'.  The canonical order is:
Invalid boolean environment override FO_MANAGED_SCRIPTING='maybe'. Use 1/0, ON/OFF, TRUE/FALSE, or YES/NO
```

The first message ends with the ten stage names in order; the root `CMakeLists.txt` must call each exactly once, in the canonical order, ending with `FinalizeProjectGeneration()`. Copy the sequence from `Examples/MinimalProject/CMakeLists.txt`. The second means an `FO_` environment variable holds a value that is not a boolean. Environment overrides outrank presets and `SetOption`, so remove the variable from the shell or profile that exports it.

## Failures documented elsewhere

| Symptom | Page |
| --- | --- |
| `ConfigBakerException: Main config baking error`, `Nothing was built in the module`, missing `WorldTime` or `GuiScreens`, `[starter-smoke] server timed out` | [First FOnline Headless Project](../tutorials/first-project.md#troubleshooting) |
| `Config file not found`, `Sub config not found`, `Resource pack name not specified`, a typo that became a custom setting | [Configure a Game Project](../how-to/build/project-configuration.md#common-failures) |
| Generated source does not compile, incremental resources stay stale, metadata sides disagree | [Generated Content Workflow](../how-to/build/generated-content.md#recovery) |
| Crashes, stack traces, script debugger and Managed C# load failures | [Native, AngelScript, and Managed C# Debugging](debugging.md) |
| Web and Android toolchain preparation | [Web](../how-to/platforms/web-debugging.md) and [Android](../how-to/platforms/android-debugging.md) build guides |
| `clang-format version 20 not found (set FO_CLANG_FORMAT or install clang-format-20 on PATH)` | [Formatter contract](../how-to/scripting/style-and-refactoring.md#formatter-contract) |

## Validation checklist

Before reporting a setup problem, confirm each item and attach the outputs:

- The first error line, copied from the log, with its call stack.
- `env --summary-only` output, with any secret values removed.
- `git submodule status Engine` and `git -C Engine rev-parse HEAD`.
- Versions of Git, Python (`py -3` or `python3`), CMake and the compiler.
- Whether the same step succeeds in the Engine-owned [first project](../tutorials/first-project.md) on the same machine.
