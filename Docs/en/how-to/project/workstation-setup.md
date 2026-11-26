---
layout: default
title: Prepare a Workstation for an FOnline Project
locale: en
document_id: project-workstation-setup
permalink: /Docs/en/how-to/project/workstation-setup.html
---

# Prepare a Workstation for an FOnline Project

This guide prepares a Windows or Linux machine (macOS to the extent Engine tooling supports it) before a game team creates or clones a repository that embeds the Engine. It names the tools to install, shows how to verify each one, explains the Engine-owned host and workspace preparation commands, and ends with a checklist whose last step is the first green build.

Generic prerequisites and the platform list live in [Build Workflow](../build/index.md#prerequisites) and the [Support Matrix](../../reference/platforms/support-matrix.md); this page does not repeat them. Commands marked for Windows were run on Windows x64. Linux and macOS commands are derived from BuildTools source and were not run on those hosts.

## Source paths inspected

- `BuildTools/README.md`
- `BuildTools/buildtools.py`
- `BuildTools/Init.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/build.cmd`
- `BuildTools/find-python.sh`
- `BuildTools/find-python.cmd`
- `BuildTools/prepare-workspace.sh`
- `BuildTools/prepare-mac-workspace.sh`
- `BuildTools/prepare-win-workspace.ps1`
- `Docs/en/reference/buildtools/commands.md`
- `Docs/en/reference/platforms/support-matrix.md`
- `Docs/en/how-to/build/index.md`
- `Docs/en/how-to/platforms/web-debugging.md`
- `Examples/MinimalProject/CMakePresets.json`
- `Examples/MinimalProject/CMakeLists.txt`
- `Source/Essentials/DiskFileSystem.cpp`
- `ThirdParty/dotnet-runtime`
- `.github/workflows/validate.yml`

## Decide what to install

Every game needs the base set. The rest depends on the script backend and the targets the project builds.

| You need | Install |
| --- | --- |
| Any project | Git, Python 3.12 or newer, CMake, a native C++ toolchain |
| Managed C# scripts | The base set plus a .NET SDK and, on Windows, an `llvm-nm` provider |
| Web client | The base set plus the pinned Emscripten SDK, Node.js and a JDK |
| Android client | The base set plus the pinned Android SDK and NDK, and a JDK 17 |
| Windows binaries from a Linux host | The base set plus the pinned MSVC SDK tree (`xwin`) |

AngelScript needs nothing beyond the base set: the compiler is built from Engine source as part of the project.

## Required tools

| Tool | Requirement | Verify |
| --- | --- | --- |
| Git | Any current release. On Windows, Git for Windows. Add Git LFS only when the project stores binaries in it. | `git --version` |
| Python 3 | 3.12 or newer. BuildTools passes `onexc=` to `shutil.rmtree`, which exists from 3.12, and every `buildtools.py` command refuses an older interpreter; Engine CI runs 3.13. CMake also needs a Python 3 interpreter that `find_package(Python3 ... Interpreter)` can find. | Windows `py -3 --version`; elsewhere `python3 --version` |
| CMake | 3.22 or newer (`cmake_minimum_required(VERSION 3.22)` in `BuildTools/Init.cmake`). The example presets declare schema `"version": 3`, which CMake 3.21 reads. | `cmake --version` |
| Windows toolchain | Visual Studio 2022 17.14 or newer with the C++ build tools (MSVC toolset 14.44, `MSVC_VERSION` 1944). | `py -3 Engine\BuildTools\buildtools.py host-check windows` |
| Linux toolchain | GCC 13 or newer, or Clang 20 or newer. | `g++ --version` or `clang++ --version` |
| macOS toolchain | Xcode with its command-line tools (any AppleClang is accepted by the configure check). | `xcode-select -p` |
| Ninja | Only when a preset selects a Ninja generator. The `linux` preset of `Examples/MinimalProject` uses `Ninja Multi-Config`. Visual Studio's CMake tools ship a copy that BuildTools finds on Windows. | `ninja --version` |
| .NET SDK | Managed C# only. The SDK must support the `ManagedScript.TargetFramework` of the project; the examples use `net10.0` and Engine CI installs `10.0.x`. | `dotnet --list-sdks` |
| `llvm-nm` | Managed C# on Windows. Install the Visual Studio component "C++ Clang tools for Windows" or LLVM, or set `FO_MANAGED_NM`. | `llvm-nm --version` |
| Node.js and a JDK | Web builds only. The Linux `web-packages` group installs `nodejs` and `default-jre`; the Windows route installs only the SDK. | `node --version`, `java -version` |
| JDK 17 | Android builds only (`openjdk-17-jdk` in the Linux `android-packages` group). | `java -version` |
| clang-format 20 | Only when the project reuses the Engine formatter wrapper; see [Formatter contract](../scripting/style-and-refactoring.md#formatter-contract). Point `FO_CLANG_FORMAT` at it if it is not on `PATH`. | `clang-format --version` |

Compiler and CMake minimums are enforced at configure time, so a machine that is too old fails with a named message instead of a late compile error; the messages are listed in [Troubleshoot Project Setup](../../troubleshooting/setup.md#toolchain-and-host-tools).

### Choose the Python interpreter

Use `py -3` on Windows and `python3` everywhere else. The name `python` is unreliable: on a machine that still has a Python 2 install ahead of Python 3 on `PATH`, `python` runs Python 2, and `buildtools.py` stops with a `SyntaxError` before it can run any check. A Python 3 older than 3.12 runs far enough to stop with `BuildTools requires Python 3.12 or newer, but <path> is Python <version>`.

The Engine wrappers pick an interpreter by running it and comparing its version with the 3.12 floor. The Windows scripts (`BuildTools/*.cmd` through `BuildTools/find-python.cmd`, and `BuildTools/prepare-win-workspace.ps1`) try `py -3`, then `python`. The POSIX scripts (`BuildTools/*.sh` through `BuildTools/find-python.sh`) try `python3`, then the newest `python3.N` on `PATH`, then `python`. A candidate below the floor, Python 2 included, is skipped; when none qualifies, the wrapper exits with code 1 and `Python 3.12 or newer not found`. The `py` launcher's `-3` selects its newest registered Python 3, so on Windows install 3.12 or newer through the official installer.

```powershell
py -3 --version
python --version
```

```bash
python3 --version
command -v python3 python
```

Every `python3` or `py -3` command in Engine documentation is interchangeable with the interpreter that passes this check. Use UTF-8 for any project file that contains non-ASCII text.

## Run the Engine host checks

BuildTools resolves its paths from the current directory, so run these commands from the root of the game repository (or from a scratch directory while learning). `env --summary-only` prints the paths a command would use and changes nothing:

```powershell
py -3 Engine\BuildTools\buildtools.py env --summary-only
```

```bash
python3 Engine/BuildTools/buildtools.py env --summary-only
```

Read `FO_PROJECT_ROOT` (the current directory by default), `FO_ENGINE_ROOT` (the parent of `BuildTools`) and `FO_WORKSPACE` (`Workspace` under the current directory). Running from a different directory silently selects a different workspace.

### host-check

`host-check` verifies the minimum tools for one host and prints `Host is ready!` on success. On failure it prints one line per problem and exits with code 1.

| Host | What it checks | Messages |
| --- | --- | --- |
| `windows` | `vswhere` finds a Visual Studio with component `Microsoft.VisualStudio.Component.VC.Tools.x86.x64`; `cmake` is on `PATH` | `Please install Visual Studio with C++ Build Tools or Build Tools for Visual Studio`, `Please install CMake from https://cmake.org` |
| `linux` | `cmake` is on `PATH` | `Please install CMake` |
| `macos` | `xcode-select -p` succeeds; `cmake` is on `PATH` or at `/Applications/CMake.app/Contents/bin/cmake` | `Please install Xcode from AppStore`, `Please install CMake from https://cmake.org` |

Before any of these rows, `buildtools.py` compares the interpreter that runs it with the 3.12 floor on every host and exits with code 1 and the message from [Choose the Python interpreter](#choose-the-python-interpreter) when it is older. Beyond that floor the check is deliberately shallow: it does not read tool versions, so an old CMake or compiler passes until configure rejects it. Treat it as a first filter, then confirm versions with the commands in the table above.

```powershell
py -3 Engine\BuildTools\buildtools.py host-check windows
```

```bash
python3 Engine/BuildTools/buildtools.py host-check linux
```

On macOS, the equivalent command is `host-check macos`. The support matrix gives macOS build gating for the desktop client only.

### prepare-host-workspace

`prepare-host-workspace <host> [feature ...] [--check]` runs the host check, creates `FO_WORKSPACE` and `FO_OUTPUT`, and prepares the requested workspace parts. With no feature it uses the host default: `all` on Linux, `toolset` on Windows, nothing on macOS. `--check` reports readiness without installing or building and exits non-zero with `Workspace part <name> is not ready (<version>)` when a part is missing or was prepared for another version.

Prepared parts record a marker file named `<part>-version.txt` in the workspace. A part is skipped while its marker matches the version BuildTools expects, so repeating a command is cheap and an Engine update that changes a pin re-prepares only the affected part.

| Host | Feature | Prepares |
| --- | --- | --- |
| Linux | `common-packages`, `linux-packages`, `web-packages`, `android-packages`, `windows-cross-packages`, `msi-packages` | Debian or Ubuntu packages through `apt-get`; `common-packages` is installed first whenever any package group is selected |
| Linux | `all-packages` | `linux-packages`, `web-packages`, `android-packages`, `windows-cross-packages` and `msi-packages` |
| Linux | `toolset`, `web`, `android-arm32`, `android-arm64`, `android-x86`, `dotnet`, `windows-cross`, `msan-libcxx` | Workspace parts: host tools tree, Emscripten, Android SDK and NDK, the .NET runtime source, the `xwin` MSVC SDK, the MemorySanitizer libc++ |
| Linux | `all` | The workspace parts `toolset`, `emscripten`, `android-sdk`, `android-ndk`, `dotnet` and `xwin`; package groups and `msan-libcxx` are not included |
| Windows | `toolset`, `web`, `wix`, `all` | Host tools tree, Emscripten, the WiX v3 toolset; `all` is all three |
| macOS | none | `prepare-host-workspace macos` takes no feature and only runs the host check |

The table is the complete vocabulary. A name outside the row set of the chosen host, for example `dotnet` on Windows, `web` on macOS, or the retired `linux` and `packages`, stops the command with exit code 2 before anything is prepared, and the message lists the names that host accepts, for example `prepare-host-workspace windows does not support dotnet: windows accepts toolset, web, wix, all (default: toolset)`.

Package installation is separate from workspace preparation by design. A feature such as `web` never installs packages, so on a fresh Linux host pass the matching `*-packages` feature first. The Linux preparer needs `apt-get` and either root or `sudo`; it adds the LLVM apt source for major version 20 when a group requires it. Hosts that are not Debian or Ubuntu install the same tools by their own means and skip the package features.

The `toolset` part configures the project in the current directory, so it requires a root `CMakeLists.txt` that includes `Engine/BuildTools/Init.cmake`. Run it after the repository exists. Whenever the part is prepared it deletes and recreates `Workspace/build-win64-toolset` (Windows) or `Workspace/build-linux-toolset` (Linux), and `buildtools.py toolset <target>` builds a target in that tree.

```powershell
py -3 Engine\BuildTools\buildtools.py prepare-host-workspace windows toolset --check
py -3 Engine\BuildTools\buildtools.py prepare-host-workspace windows toolset
```

```bash
python3 Engine/BuildTools/buildtools.py prepare-host-workspace linux common-packages linux-packages
python3 Engine/BuildTools/buildtools.py prepare-host-workspace linux toolset --check
```

### prepare-workspace

`prepare-workspace <part> [part ...] [--check]` prepares workspace parts directly, without the host check or the package groups. The parts are `toolset`, `emscripten`, `android-sdk`, `android-ndk`, `dotnet`, `xwin`, `wix` and `msan-libcxx`. Use it when the host mapping above does not offer a part for your OS. The `dotnet` part, for example, clones the pinned .NET runtime source into `Workspace/dotnet/runtime` and is reachable on Windows only through this command:

```powershell
py -3 Engine\BuildTools\buildtools.py prepare-workspace dotnet --check
```

```bash
python3 Engine/BuildTools/buildtools.py prepare-workspace emscripten android-ndk dotnet
```

Managed C# needs a .NET SDK for scripts in addition to this runtime source; the part does not install an SDK. `msan-libcxx` is Linux-only and is a contributor concern. `xwin` prepares the MSVC SDK on a Linux host, and `wix` prepares the WiX v3 toolset on Windows only. Platform procedures that need more than the base set are owned by [Web](../platforms/web-debugging.md), [Android](../platforms/android-debugging.md) and [Managed C#](../scripting/managed-csharp.md).

### Wrapper scripts

Three scripts call `prepare-host-workspace` for you. They add the Python lookup described above and a `check` word that becomes `--check`; every other argument is forwarded unchanged, so the wrappers accept exactly the features in the table for their host and fail the same way on any other name.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Engine\BuildTools\prepare-win-workspace.ps1 toolset check
```

```bash
bash Engine/BuildTools/prepare-workspace.sh common-packages linux-packages toolset
bash Engine/BuildTools/prepare-mac-workspace.sh check
```

Wrapper-specific behavior:

- `prepare-workspace.sh` refuses to run without an argument; pass at least one feature or `check`. `prepare-win-workspace.ps1` with no argument prepares the Windows default `toolset`.
- `prepare-win-workspace.ps1` prints `Workspace is not ready!` and exits with code 10 in `check` mode when a part is missing. Any other failure, including an unknown feature, exits with code 1.
- `prepare-mac-workspace.sh` has no workspace parts to prepare. When the host check fails outside `check` mode, it waits for Enter and repeats instead of exiting; an unknown feature exits at once with code 2.

This page calls `buildtools.py` directly, which accepts the same vocabulary as the wrappers.

## Workspace and cache variables

BuildTools reads only variables with the `FO_` prefix. The first five rows decide where files go; set them before the first command and keep them stable, because the workspace markers live there.

| Variable | Default | Effect |
| --- | --- | --- |
| `FO_PROJECT_ROOT` | current directory | Repository root: used for the `toolset` configure, the Git queries and checkout case repair |
| `FO_ENGINE_ROOT` | parent of `BuildTools` | Engine checkout |
| `FO_WORKSPACE` | `Workspace` under the current directory | Prepared SDKs, markers and tool build trees |
| `FO_OUTPUT` | `output` under the workspace | Output tree for BuildTools-driven builds |
| `FO_BINARY_OUTPUT_POSTFIX` | empty | Postfix on binary output directory names |
| `FO_DOWNLOAD_MIRROR` | unset | Base URL of a pull-through mirror for archive downloads |
| `FO_WORKSPACE_CACHE` | unset | Base URL for prepared Emscripten and MSVC SDK trees, built once and downloaded whole |
| `FO_CI_TOKEN` | unset | Bearer token sent only to the host of the two URLs above |
| `FO_CI_CA` | unset | Path to a PEM file of extra trust anchors, added to the system store |
| `FO_CLANG_FORMAT` | search `PATH` | Explicit `clang-format` binary for `format-source` |
| `FO_DOTNET_DIR` | `dotnet` under the build tree | Managed runtime build workspace; point it at a short path on Windows |
| `FO_MANAGED_RUNTIME_PREBUILT` | unset | Published `output/mono/<triplet>` tree; required to target Windows from a non-Windows host |
| `FO_ANDROID_HOME`, `FO_ANDROID_SDK_ROOT`, `FO_ANDROID_NDK_ROOT` | the prepared workspace parts | Explicit Android SDK and NDK locations |

Mirror semantics, the content-length check and the cache-miss rule are described in [Build Workflow](../build/index.md#fetching-through-a-mirror-of-your-own); CI usage and secret handling are in [Set Up Continuous Integration for a Game Project](continuous-integration.md). The mirror covers archive downloads. `git clone` calls made by workspace preparation, such as the Emscripten SDK and the .NET runtime source, contact GitHub directly.

Any project option declared in `BuildTools/cmake/ProjectInterface.json` can also be overridden by an environment variable of the same name, and that override outranks `-D` values, presets and `SetOption` defaults. A stale `FO_*` variable left in a shell profile therefore changes a build without appearing in the project files. List the `FO_` variables of a shell when a configure result surprises you:

```powershell
Get-ChildItem Env: | Where-Object { $_.Name -like 'FO_*' }
```

```bash
env | grep '^FO_'
```

## Windows long paths and links

### Long paths

Engine and Git handle long paths in different ways, and a project has to cover the parts Engine does not.

- Engine native code switches to extended-length paths for long names on Windows (`Source/Essentials/DiskFileSystem.cpp`), so runtime and baker file access does not hit `MAX_PATH` by itself.
- `buildtools.py` passes `core.longpaths=true` only to its own clone of the .NET runtime source. It does not change your Git configuration, your repository, or the Windows registry.
- Compilers, MSBuild and the .NET runtime build still see paths in the build tree. The managed runtime build documents a failure at 297 characters with `C1083: Cannot open compiler generated file`, which is why `FO_DOTNET_DIR` exists as an override.
- At this Engine revision, the longest tracked path inside `Engine/` is 155 characters. Your checkout root, `Engine/` and the build directory name are added on top.

Configure these before cloning:

```powershell
git config --global core.longpaths true
```

Also keep the checkout root short, for example `C:\fo\game`, and enable the Windows "Enable Win32 long paths" policy (`LongPathsEnabled` under `HKLM\SYSTEM\CurrentControlSet\Control\FileSystem`) if your tools support it. The registry value is an operating-system setting that Engine neither sets nor verifies. For a Managed C# project, also set `FO_DOTNET_DIR` to a short directory such as `C:\fo\dotnet`.

### Symlinks and junctions

Engine itself needs few links, and a project decides how many more it adds.

- The Engine repository tracks no symbolic links, so cloning it needs no link privilege.
- `buildtools.py validate` creates an `Engine` link inside its scratch project: `mklink /d` on Windows, a symbolic link on POSIX. Windows allows `mklink /d` for administrators or when Developer Mode is on.
- The Engine-owned examples need an `Engine` entry beside their `CMakeLists.txt` when they run in place. The [first project tutorial](../../tutorials/first-project.md) creates it as a junction (`New-Item -ItemType Junction`), which needs no special privilege.
- A project that tracks symbolic links in its own repository, for example an editor-extension folder that points into `Engine/BuildTools/`, needs `core.symlinks` set to `true` and the link permission above. Without them, Git checks a symbolic link out as a plain text file that contains the target path, and the tool that expects a directory fails. Check with `git config --get core.symlinks` and recreate the checkout after fixing the setting.

## Verify the workstation

Work through this list on every new machine. Items 1 to 4 need no project.

1. `git --version` reports a release you intend to use, and `git config --get core.longpaths` prints `true` on Windows.
2. `py -3 --version` (Windows) or `python3 --version` (elsewhere) reports 3.12 or newer.
3. `cmake --version` reports 3.22 or newer, preferably 3.28 or newer.
4. `buildtools.py host-check <host>` prints `Host is ready!`, and your compiler meets the minimum in the tools table.
5. Managed C# only: `dotnet --list-sdks` lists an SDK that matches the project target framework, and `llvm-nm` resolves on Windows.
6. The repository root is short on Windows, and `env --summary-only` prints the workspace and output paths you expect.
7. From the project root, `prepare-host-workspace <host> toolset --check` either succeeds or reports a part you then prepare without `--check`.
8. Run the Engine-owned first build from the [first project tutorial](../../tutorials/first-project.md). It configures and builds the baker and headless server, bakes, runs the server and requires the lifecycle markers; a green run proves the toolchain, Python, CMake and the baking path together.

After step 8, continue with [Create an FOnline Game Repository](create-repository.md) to build your own project, and keep [Troubleshoot Project Setup](../../troubleshooting/setup.md) open for any failure on the way. For runtime or debugger failures after a successful build, use [Native, AngelScript, and Managed C# Debugging](../../troubleshooting/debugging.md).

## Validation checklist

- Every tool in the "Required tools" table has a version printed on the new machine.
- `host-check` exits with code 0 and prints `Host is ready!`.
- `env --summary-only` shows the intended project root, Engine root and workspace.
- The first project tutorial finished with exit code 0 and all required markers.
- Windows only: the long-path Git setting is on and the checkout root is short.
- Any project-tracked symbolic links exist as links, not as text files.
