---
layout: default
title: Set Up a VS Code Workspace for an FOnline Project
locale: en
document_id: project-vscode-workspace
permalink: /Docs/en/how-to/project/vscode-workspace.html
---

# Set Up a VS Code Workspace for an FOnline Project

This guide shows how to configure Visual Studio Code for an embedding project that uses the Engine-owned extensions FOnline Tools and the AngelScript debugger. It teaches the task and launch label grammar that the FOnline Tools menu reads from the project's `.vscode/tasks.json` and `.vscode/launch.json`, a recommended task graph, launch and attach profiles, and workspace settings. Both extensions are working Engine-owned tools, delivered as source from `BuildTools` and installed in a project through tracked workspace symlinks. The label grammar is defined by the FOnline Tools source.

## Source paths inspected

- `.vscode/extensions.json`
- `.gitignore`
- `.github/workflows/validate.yml`
- `BuildTools/fonline-tools/package.json`
- `BuildTools/fonline-tools/tsconfig.json`
- `BuildTools/fonline-tools/src/extension.ts`
- `BuildTools/fonline-tools/src/jsonc.ts`
- `BuildTools/fonline-tools/src/tests/jsonc.test.ts`
- `BuildTools/fonline-tools/dist/extension.js`
- `BuildTools/angelscript-debugger/package.json`
- `BuildTools/angelscript-debugger/src/extension.ts`
- `BuildTools/angelscript-debugger/src/activateFosDebug.ts`
- `BuildTools/angelscript-debugger/src/attachDiscovery.ts`
- `BuildTools/angelscript-debugger/src/fosDebug.ts`
- `BuildTools/angelscript-debugger/src/fosRuntime.ts`
- `BuildTools/angelscript-debugger/src/tests/adapter.test.ts`
- `BuildTools/angelscript-debugger/src/tests/attachDiscovery.test.ts`
- `BuildTools/tests/test_docs_debugging.py`
- `BuildTools/build.cmd`
- `BuildTools/build.sh`
- `BuildTools/buildtools.py`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `Source/Common/Common.h`
- `Source/Common/Common.cpp`
- `Examples/MinimalProject/CMakeLists.txt`
- `Examples/MinimalProject/CMakePresets.json`
- `Examples/MinimalProject/FOnlineStarter.fomain`
- `Examples/MinimalProject/README.md`
- `Docs/en/troubleshooting/debugging.md`
- `Docs/en/reference/prototype-format/syntax.md`

## What the Engine ships

| Item | Extension id | Role | Entry point |
| --- | --- | --- | --- |
| FOnline Tools | `fodev.fonline-tools` | A quick-pick menu, status bar entry and numbered hotkeys over the workspace tasks and launch configurations | `BuildTools/fonline-tools`, `main` is `dist/extension.js` |
| FOnline AngelScript | `fodev.fo-angelscript` | The `fos` language for `.fos`, the `fos` debugger type, attach discovery | `BuildTools/angelscript-debugger`, `main` is `dist/extension.js` |
| Recommendations | not an extension | `ms-vscode.cpptools` and `ms-vscode.cmake-tools` in the Engine's own `.vscode/extensions.json` | `.vscode/extensions.json` |

Both extensions are `private`, version `0.1.0`, require VS Code `^1.89.0`, and declare `extensionKind` `workspace`. The repository contains no VSIX and no marketplace publication record. The compiled `dist/` bundle is committed, so the extension folders can be loaded without running npm.

The Engine's `.vscode/extensions.json` applies when the Engine repository itself is the opened folder. A project opened at its own root needs its own `.vscode/extensions.json`; see [Recommended extensions](#recommended-extensions).

## Contract and test coverage

FOnline Tools and the AngelScript debugger are working Engine-owned tools. The FOnline Tools label grammar has no schema and no generated reference: the source in `BuildTools/fonline-tools/src/extension.ts` is the specification, and a change to it can change which entries your workspace shows. Engine changes to the grammar are recorded in the changelog with their consequence for existing `tasks.json` and `launch.json` files.

Automated coverage in the Engine repository:

- `npm test` in `BuildTools/fonline-tools` typechecks the extension and unit-tests its `tasks.json` parser. Menu building, platform filtering and hotkeys need the VS Code API and have no automated test.
- `npm test` in `BuildTools/angelscript-debugger` typechecks the package, rebuilds `dist/`, drives the bundled adapter over the Debug Adapter Protocol against its sample runtime, and unit-tests attach target selection. No test attaches to a live Engine process or runs UDP discovery.
- Both test scripts run on a developer machine. `.github/workflows/validate.yml` runs neither of them; the only Engine check that touches the packages is `BuildTools/tests/test_docs_debugging.py`, which asserts documentation claims about the debugger's `package.json` and the absence of a lock file and VSIX. Details are in [Maintain the FOnline VS Code Extensions](../../contributing/tooling/vscode-extensions.md).

The behavior in this guide was derived from the source, and the menu and hotkey rules were checked by running the bundle against a stub of the VS Code API. The Engine repository does not exercise either extension inside a real VS Code window.

For the adapter's own contract, supported attach controls and security boundary, use [Native, AngelScript, and Managed C# Debugging](../../troubleshooting/debugging.md#angelscript-debugger).

## Install the extensions

### Prerequisites

1. Git with `core.symlinks` set to `true` for the clone. Check with `git config core.symlinks`.
2. On Windows, permission to create symbolic links: either Developer Mode or an account that holds the symlink privilege. Git then writes real links on checkout.
3. VS Code `1.89` or newer.
4. A trusted workspace. Neither `package.json` declares untrusted-workspace support, so trust the project folder; the exact restricted-mode behavior of VS Code was not verified.

Without symlink support Git checks a symbolic link out as a small text file holding the link target, and VS Code finds no extension folder. This is Git behavior and was not reproduced for this guide. On a Windows host with symlink permission, Git created a real directory symbolic link for a tracked mode `120000` entry.

### Add the tracked symlinks

A project reaches the Engine extensions through two tracked symbolic links, so every clone gets the same extension folders without a copy:

```text
.vscode/extensions/fonline-tools        -> ../../Engine/BuildTools/fonline-tools
.vscode/extensions/angelscript-debugger -> ../../Engine/BuildTools/angelscript-debugger
```

Create them once with Git itself, which works the same in Git Bash on Windows and in bash on Linux:

```bash
mkdir -p .vscode/extensions
for name in fonline-tools angelscript-debugger; do
  blob=$(printf '%s' "../../Engine/BuildTools/$name" | git hash-object -w --stdin)
  git update-index --add --cacheinfo "120000,$blob,.vscode/extensions/$name"
  git checkout-index -f -- ".vscode/extensions/$name"
done
```

On Linux, `ln -s ../../Engine/BuildTools/fonline-tools .vscode/extensions/fonline-tools` followed by `git add` is equivalent. In PowerShell, `New-Item -ItemType SymbolicLink -Target` resolves a relative target against the current directory, not against the link's folder, so a link made that way does not point at the Engine. Use the Git route above.

Verify the links:

```powershell
git config core.symlinks
Get-Item .vscode\extensions\fonline-tools | Select-Object LinkType, Target
```

```bash
git config core.symlinks
ls -l .vscode/extensions
```

If a clone made without symlink support left plain files behind, enable symlinks and restore the entries:

```bash
git config core.symlinks true
rm .vscode/extensions/fonline-tools .vscode/extensions/angelscript-debugger
git checkout -- .vscode/extensions
```

### Confirm the extensions load

The Engine contains no code that loads the symlinked folders: loading the directories under `<workspace>/.vscode/extensions` is VS Code behavior, and embedding projects use this symlink route in daily work. The Engine repository has no automated test of it, so confirm the result once per clone and VS Code version:

1. Open the project folder and accept the workspace trust prompt.
2. Look for the `FOnline Tools` entry at the left of the status bar and the `FOnline: Show Tools` command in the Command Palette.
3. Run `Developer: Show Running Extensions` and confirm `FOnline Tools` and `FOnline AngelScript` are listed.
4. If they are only offered as recommendations, install them from the Extensions view prompt.

The `fos` extension activates lazily on debug events (`onDebugResolve:fos`), so it may be absent from the running list until a `fos` debug session starts.

## How FOnline Tools builds its menu

The extension activates on `onStartupFinished`, registers the command `FOnline: Show Tools` (`fonline-tools.showTools`) and shows a status bar entry `FOnline Tools` at the left edge of the status bar. Clicking the entry runs the same command. The command opens a quick pick that mixes tasks and launch configurations.

### Data sources

- Tasks are read from `.vscode/tasks.json` of the first workspace folder only. The extension reads the file itself and accepts the same relaxed JSON as VS Code: `//` and `/* */` comments, trailing commas before `}` or `]`, and a leading byte-order mark. A missing file means the menu has no tasks. A file that still does not parse, such as one with an unclosed bracket, is reported as the error `FOnline Tools: Cannot parse <path>: <reason>`, and neither the menu nor a hotkey runs until the file is fixed.
- The extension then asks VS Code for the workspace tasks and keeps those whose name equals a label from that file and whose scope is the workspace or the first folder.
- Launch configurations and compounds come from VS Code's configuration API for `launch` in the first workspace folder, so `launch.json` may contain comments.
- In a multi-root workspace, tasks and launch entries of other folders are never listed.

A task without a `label` is never listed.

### Label grammar

Every control is written inside the task `label` or the launch `name`. Tags stay in the string VS Code stores, so `dependsOn`, `preLaunchTask` and compound `configurations` must repeat the complete label, tags included.

| Element | Spelling | Effect |
| --- | --- | --- |
| Group | `Group :: Item` | Entries sharing `Group` move into a submenu shown as `Group ...`. Only the first `::` splits, and the split needs text on both sides after tags are removed. A label that starts with `::` is not grouped. Only one level exists, so `A :: B :: C` is group `A`, item `B :: C`. |
| Hidden | `[hidden]` | The entry is omitted from the menu and from the hotkeys. The task or configuration still exists for VS Code, `dependsOn`, `preLaunchTask` and compounds. |
| Platform tag | `[windows]`, `[linux]`, `[macos]` | The entry is shown only on the listed host platforms. Several tags are a union. |
| Platform tag aliases | `[win32]`, `[win64]` mean windows; `[osx]`, `[darwin]` mean macOS | Tags are case-insensitive. |
| Sort tag | `[sort:N]` with digits | Orders entries, and binds hotkey `Ctrl+Shift+N` for `N` from 1 to 9. |
| Platform qualifier | `(Windows)`, `(Linux x64)` | A parenthesized group whose first word is one of the platform names or aliases above restricts the host. Other parentheses such as `(Debug)` are ignored. The qualifier stays in the displayed label. |
| Per-OS blocks | `windows`, `linux`, `osx` keys on the task or configuration | The presence of a key marks that platform as supported; see the precedence rules below. |
| Host-limited debugger type | `"type": "cppvsdbg"` | A launch configuration of this type is shown only on Windows. It is the only type the extension knows. |

Tags are removed from the displayed label and runs of whitespace collapse, so `Server [windows] [sort:3]` displays as `Server`. Display names must still be unique in `launch.json`, which is why host variants differ by tag.

### Platform filtering rules

The host is the Node `process.platform`: `win32` is windows, `linux` is linux, `darwin` is macOS. Any other value, or a missing `process`, disables filtering and every entry is shown. For one entry the first rule that applies decides:

1. If the label has platform tags, the host must be among them.
2. Otherwise, if the label has recognized parenthesized qualifiers, the host must be among them.
3. Otherwise, if the entry has any `windows`, `linux` or `osx` key, the host must be among the keys present.
4. Otherwise the entry is shown.

A launch configuration must also pass the debugger-type rule: `cppvsdbg` requires a Windows host in addition to the label rules. A compound uses the label rules, needs a non-empty `configurations` list, and every listed name must be a plain string equal to the name of a launch configuration that is supported on this host. A compound that references a Windows-only configuration therefore disappears on Linux, a hidden configuration still counts as supported, and a `{ "name": ..., "folder": ... }` object entry makes the compound disappear.

Consequences worth planning for:

- A task that only has a `windows` override block is visible only on Windows. To keep a task visible on every host, give it all three blocks, or none.
- A task with `windows` and `linux` blocks and no `osx` block is hidden on macOS, which is the intended result for a project without macOS presets.
- Composite tasks that only list `dependsOn` have no blocks and stay visible on every host, including hosts where their dependencies are hidden. Add `[windows]` and `[linux]` tags to such a task if it must follow its dependencies.

### Ordering and submenus

The top level lists direct entries first, then group entries. Inside a list the order is: entries with `[sort:N]` by ascending `N`, then the rest by label with locale-aware comparison. Task entries carry the description `Task`, launch configurations `Launch`, compounds `Launch Compound`, and submenus `Task Group` or `Launch Group`. Typing filters on label, description and detail.

Tasks and launch configurations are grouped separately. A task group `Build` and a launch group `Build` appear as two submenu entries. A `[sort:N]` on an item inside a group orders that item within its submenu only.

### Hotkeys

`Ctrl+Shift+1` through `Ctrl+Shift+9` run `FOnline: Run Sorted Item 1` to `9` (`fonline-tools.runSorted1` to `runSorted9`). They are active when neither the Quick Open widget nor the suggestion widget is showing. Remap the command ids in `keybindings.json` if a chord collides with another extension. Whether the default chords collide with VS Code's own shortcuts on your layout was not checked.

A hotkey runs the top-level entry whose `[sort:N]` equals the digit:

- Entries inside a group never get a hotkey. Keep hotkey targets as direct entries.
- Hidden entries have no hotkey.
- `N` of 0 or 10 and above only orders the menu.
- If two entries share a number, the one listed first in the menu runs, which is the one whose label sorts first. Use unique numbers.
- When no entry matches, a warning `No FOnline item with [sort:N] found.` is shown.

Hotkeys keep no cache. Every press rebuilds the same top-level list that `FOnline: Show Tools` shows, from the current `tasks.json`, the tasks VS Code returns and the current `launch.json`, so an edit takes effect on the next press without reopening the menu or reloading the window.

## Recommended task graph

Keep the graph small and uniform so the menu stays readable and tasks never run in the wrong order:

1. A hidden `Configure` task runs the CMake configure step for the host preset. Every build, bake and test task lists it in `dependsOn`.
2. Build and bake tasks are grouped (`Build :: ...`, `Test :: ...`) and call `cmake --build <build dir> --config <config> --target <target>` with an explicit target.
3. `Prepare` bakes resources and builds the executables a launch needs. It uses `dependsOrder: sequence` because the steps share one build tree.
4. Launch configurations use `preLaunchTask` set to the `Prepare` label.
5. An aggregate `Verify` task runs `Prepare` and then the project's smoke or test target, in sequence.
6. Interpreter-driven tasks pick the interpreter per host, as shown next.

Do not let two children that write the same build or bake output run in parallel. VS Code runs `dependsOn` children in parallel unless `dependsOrder` is `sequence`. When several tasks depend on `Configure`, it can run more than once in a sequence; it is idempotent, but this guide did not check whether VS Code deduplicates it.

For concrete targets, use the names your project generates. The example below follows the Engine-owned minimal project: preset names `windows` and `linux`, binary directory `Build/<preset>`, development name `FOSTART`, targets `BakeResources`, `FOSTART_ServerHeadless` and `RunStarterSmoke`.

### Choose the Python interpreter per OS

Do not put a bare `python` in a shared task. On a Windows development host the name can resolve to Python 2, and the Engine's BuildTools scripts do not parse under Python 2. `BuildTools/build.cmd` prefers the `py -3` launcher and falls back to `python`. `BuildTools/build.sh` prefers `python3` and falls back to `python`. Mirror that in tasks with per-OS blocks: `py` with `-3` as the first argument on Windows and `python3` on Linux. A task written this way has `windows` and `linux` blocks, so it is not shown on macOS.

BuildTools offers `buildtools.py host-check {linux,macos,windows}` and `buildtools.py build <platform> <target> [config]`; see the [BuildTools command reference](../../reference/buildtools/commands.md).

## Launch profiles

Keep three kinds of profile, all driven from `launch.json`:

- Native launch under a debugger with `preLaunchTask`. Use `cppvsdbg` on Windows and `cppdbg` with GDB or LLDB on Linux, both from the `ms-vscode.cpptools` extension, whose fields it owns. Point `program` at the exact generated executable and `cwd` at the directory the build uses. Engine applications are built with a working directory equal to the output path (`FO_OUTPUT_PATH`); the minimal project sets it to its CMake binary directory.
- AngelScript attach with type `fos` and request `attach`. Fields are `endpoint`, `discoveryPort`, `discoveryTimeoutMs` and `processId`.
- A compound of the two when you want to start the process and attach in one action.

Rules specific to the `fos` adapter, derived from its source:

- Use `fos` only for `attach`. Its `launch` request runs the adapter's sample runtime, which reads `program` as a text file; it does not start an Engine process.
- The process must run with `AngelScript.DebuggerEnabled` true. The example passes `--AngelScript.DebuggerEnabled True` on the command line; the setting reference and security boundary are in [AngelScript debugger](../../troubleshooting/debugging.md#angelscript-debugger).
- When `endpoint` is not set, the adapter runs UDP discovery. Without `processId` it shows a picker of every discovered target and stores the chosen `endpoint` and `processId`. A `processId` in `launch.json` filters the discovered targets by exact match with the advertised `<pid>:<tcp-port>` id, so a bare process id matches nothing: one match attaches without a picker, several matches (the same id from several hosts) are offered in the picker, and no match shows a warning and cancels the session. Set `endpoint` (for example `tcp://127.0.0.1:43042`) to skip discovery.
- Discovery runs when the attach session starts. In a compound that also launches the process, the endpoint must already be open at that moment, or discovery reports no targets. VS Code's start timing for compounds was not exercised here.

The fields, capability matrix, security rules and checklist for a maintained profile are in [Debugger integration in an embedding project](../../troubleshooting/debugging.md#debugger-integration-in-an-embedding-project) and [Project launch-profile checklist](../../troubleshooting/debugging.md#project-launch-profile-checklist). Do not copy them into project docs.

## Complete minimal example

The two files below are plain JSON; comments and trailing commas would be accepted as well. Replace the preset, directory and target names with your project's. The `serverExecutable` input prompts for the executable path: for a Release build the Engine places a server application at `<FO_OUTPUT_PATH>/Binaries/Server-<FO_BUILD_PLATFORM>/<DevName>_ServerHeadless`, with `Windows-win64` as the platform on 64-bit Windows and a `.exe` suffix. That path was derived from the CMake sources, not observed from a build. Replace the input with a literal path once you have confirmed it in your build tree. The `-ApplySubConfig StarterSmoke` argument selects the sub-config of the minimal project; use your own.

`.vscode/tasks.json`:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Configure [hidden]",
      "type": "shell",
      "command": "cmake",
      "windows": { "args": ["--preset", "windows"] },
      "linux": { "args": ["--preset", "linux"] },
      "problemMatcher": []
    },
    {
      "label": "Build :: Bake Resources",
      "type": "shell",
      "command": "cmake",
      "windows": { "args": ["--build", "Build/windows", "--config", "Release", "--target", "BakeResources"] },
      "linux": { "args": ["--build", "Build/linux", "--config", "Release", "--target", "BakeResources"] },
      "dependsOn": ["Configure [hidden]"],
      "problemMatcher": []
    },
    {
      "label": "Build :: Server Headless",
      "type": "shell",
      "command": "cmake",
      "windows": { "args": ["--build", "Build/windows", "--config", "Release", "--target", "FOSTART_ServerHeadless"] },
      "linux": { "args": ["--build", "Build/linux", "--config", "Release", "--target", "FOSTART_ServerHeadless"] },
      "dependsOn": ["Configure [hidden]"],
      "problemMatcher": []
    },
    {
      "label": "Test :: Starter Smoke",
      "type": "shell",
      "command": "cmake",
      "windows": { "args": ["--build", "Build/windows", "--config", "Release", "--target", "RunStarterSmoke"] },
      "linux": { "args": ["--build", "Build/linux", "--config", "Release", "--target", "RunStarterSmoke"] },
      "dependsOn": ["Configure [hidden]"],
      "problemMatcher": []
    },
    {
      "label": "Tools :: Host Check",
      "type": "shell",
      "windows": { "command": "py", "args": ["-3", "Engine/BuildTools/buildtools.py", "host-check", "windows"] },
      "linux": { "command": "python3", "args": ["Engine/BuildTools/buildtools.py", "host-check", "linux"] },
      "problemMatcher": []
    },
    {
      "label": "Prepare [sort:1]",
      "dependsOn": ["Build :: Bake Resources", "Build :: Server Headless"],
      "dependsOrder": "sequence"
    },
    {
      "label": "Verify [sort:2]",
      "dependsOn": ["Prepare [sort:1]", "Test :: Starter Smoke"],
      "dependsOrder": "sequence"
    }
  ]
}
```

`.vscode/launch.json`:

```json
{
  "version": "0.2.0",
  "inputs": [
    {
      "id": "serverExecutable",
      "type": "promptString",
      "description": "Path to the built FOSTART_ServerHeadless executable"
    }
  ],
  "configurations": [
    {
      "name": "Server [windows] [sort:3]",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${input:serverExecutable}",
      "args": ["-ApplyConfig", "${workspaceFolder}/FOnlineStarter.fomain", "-ApplySubConfig", "StarterSmoke"],
      "cwd": "${workspaceFolder}/Build/windows",
      "preLaunchTask": "Prepare [sort:1]"
    },
    {
      "name": "Server [linux] [sort:3]",
      "type": "cppdbg",
      "request": "launch",
      "MIMode": "gdb",
      "program": "${input:serverExecutable}",
      "args": ["-ApplyConfig", "${workspaceFolder}/FOnlineStarter.fomain", "-ApplySubConfig", "StarterSmoke"],
      "cwd": "${workspaceFolder}/Build/linux",
      "preLaunchTask": "Prepare [sort:1]"
    },
    {
      "name": "Server With Script Debugger [windows] [hidden]",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${input:serverExecutable}",
      "args": ["-ApplyConfig", "${workspaceFolder}/FOnlineStarter.fomain", "-ApplySubConfig", "StarterSmoke", "--AngelScript.DebuggerEnabled", "True"],
      "cwd": "${workspaceFolder}/Build/windows",
      "preLaunchTask": "Prepare [sort:1]"
    },
    {
      "name": "Server With Script Debugger [linux] [hidden]",
      "type": "cppdbg",
      "request": "launch",
      "MIMode": "gdb",
      "program": "${input:serverExecutable}",
      "args": ["-ApplyConfig", "${workspaceFolder}/FOnlineStarter.fomain", "-ApplySubConfig", "StarterSmoke", "--AngelScript.DebuggerEnabled", "True"],
      "cwd": "${workspaceFolder}/Build/linux",
      "preLaunchTask": "Prepare [sort:1]"
    },
    {
      "name": "Attach AngelScript [sort:5]",
      "type": "fos",
      "request": "attach",
      "discoveryPort": 43001,
      "discoveryTimeoutMs": 800
    }
  ],
  "compounds": [
    {
      "name": "Server And Script Attach [windows] [sort:4]",
      "configurations": ["Server With Script Debugger [windows] [hidden]", "Attach AngelScript [sort:5]"]
    },
    {
      "name": "Server And Script Attach [linux] [sort:4]",
      "configurations": ["Server With Script Debugger [linux] [hidden]", "Attach AngelScript [sort:5]"]
    }
  ]
}
```

Resulting menu on a Windows or Linux host: top-level `Prepare`, `Verify`, `Server`, `Server And Script Attach` and `Attach AngelScript`, then the submenus `Build ...`, `Test ...` and `Tools ...`. Hotkeys `Ctrl+Shift+1` to `Ctrl+Shift+5` run those five entries in that order. On macOS only `Prepare`, `Verify` and `Attach AngelScript` remain, and `Prepare` would fail because its dependencies are hidden there: add `[windows] [linux]` tags to the two composite labels if that matters, and update every `dependsOn` and `preLaunchTask` reference to match.

A composite task that has only `dependsOn` is listed by the extension only if VS Code returns it from its task query. That was not exercised in a real VS Code window.

## Workspace settings and extensions

### Recommended extensions

The Engine's own `.vscode/extensions.json` recommends `ms-vscode.cpptools` (native debugging, which supplies `cppvsdbg` and `cppdbg`) and `ms-vscode.cmake-tools` (CMake presets). Repeat them in the project's `.vscode/extensions.json`:

```json
{
  "recommendations": [
    "ms-vscode.cpptools",
    "ms-vscode.cmake-tools"
  ]
}
```

Do not list `fodev.fonline-tools` or `fodev.fo-angelscript` there: they are not marketplace extensions and are delivered by the symlinks.

### Workspace `settings.json`

Suggested settings. The first block maps FOnline text formats that use the shared configuration syntax (`.fomain`, prototype `.fopro`, map `.fomap`, with `#` comments) to VS Code's INI mode. Do not map `.fotxt`, which is a brace-delimited format with no comment syntax. Do not remap `.fos`, which the AngelScript extension claims. The exclude lists mirror the generated trees that the minimal project ignores in Git. `cmake.useCMakePresets` is a CMake Tools setting; this guide did not verify the extension's behavior.

```json
{
  "files.associations": {
    "*.fomain": "ini",
    "*.fopro": "ini",
    "*.fomap": "ini"
  },
  "files.watcherExclude": {
    "**/Build/**": true,
    "**/Baking/**": true,
    "**/Cache/**": true,
    "**/Binaries/**": true,
    "**/Workspace/**": true
  },
  "search.exclude": {
    "**/Build/**": true,
    "**/Baking/**": true,
    "**/Cache/**": true,
    "**/Binaries/**": true,
    "**/Workspace/**": true
  },
  "cmake.useCMakePresets": "always"
}
```

Add your project's extra prototype file extensions to `files.associations` if it extends `Baking.ProtoFileExtensions`. Keep `Engine/ThirdParty` out of `search.exclude` only if you intentionally search vendored libraries.

## Visual Studio, Natvis and other IDEs

- Visual Studio: a generated MSVC solution attaches the Engine Natvis and NatJMC files automatically, and application targets set the debugger working directory to `FO_OUTPUT_PATH`. See [Visual Studio Visualizers](../../troubleshooting/debugging.md#visual-studio-visualizers). How to attach the Natvis files to a VS Code `cppvsdbg` session was not checked.
- Linux and macOS native debugging use GDB or LLDB against the exact executable; the Engine does not claim a checked macOS editor profile. See [debugging by platform](../../troubleshooting/debugging.md#windows).
- JetBrains and other editors: the Engine's `.gitignore` excludes `.idea`, `.theia`, `.vs` and `.cache` local state but ships no checked profile for them. The AngelScript attach protocol is documented in the debugging guide for any client that implements it; the `fos` adapter itself is VS Code only.

## Validation checklist

1. `git config core.symlinks` prints `true` and both extension links resolve to the Engine folders.
2. `FOnline Tools` appears in the status bar and `FOnline: Show Tools` lists the expected entries for the current OS.
3. Opening `FOnline: Show Tools` reports no `Cannot parse` error for `tasks.json`, and every `dependsOn`, `preLaunchTask` and compound name matches a label character for character, including tags.
4. A hidden `Configure` task is the dependency of every build, bake and test task.
5. `Prepare` bakes and builds in sequence; `Verify` runs `Prepare` and then the smoke target.
6. Each hotkey target is a direct entry with `[sort:N]` from 1 to 9, and none is hidden.
7. Python tasks use `py -3` on Windows and `python3` on Linux.
8. A native launch stops at a breakpoint, and the `fos` attach finds the process and stops on a script breakpoint, following the [launch-profile checklist](../../troubleshooting/debugging.md#project-launch-profile-checklist).

## See also

- [Maintain the FOnline VS Code Extensions](../../contributing/tooling/vscode-extensions.md)
- [Native, AngelScript, and Managed C# Debugging](../../troubleshooting/debugging.md)
- [Embedding FOnline in a Game Project](../build/embedding-project.md)
- [Workstation setup](workstation-setup.md)
- [Quality gates](quality-gates.md)
- [Continuous integration](continuous-integration.md)
- [Project tests](project-tests.md)
- [Project lifecycle index](index.md)
