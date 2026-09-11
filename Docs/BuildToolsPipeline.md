# BuildTools Pipeline

This document explains the staged CMake pipeline under `BuildTools/cmake/`. It is a source-grounded companion to [BuildWorkflow.md](BuildWorkflow.md): use `BuildWorkflow.md` for how to approach builds as a user, and this file for where the reusable build machinery lives.

## Ownership model

FOnline is normally configured from an embedding game project. The engine supplies CMake stages and helpers; the game project supplies values such as product names, main config, enabled targets, output paths, packages, scripts, and platform choices.


## Source paths inspected

- `BuildTools/Init.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/ProjectOptions.cmake`
- `BuildTools/cmake/stages/ThirdParty.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/CoreLibs.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/Packages.cmake`
- `BuildTools/cmake/stages/Finalize.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/Commands.cmake`
- `BuildTools/cmake/helpers/Options.cmake`
- `BuildTools/cmake/helpers/State.cmake`
- `BuildTools/cmake/helpers/WriteBuildHash.cmake`
- `BuildTools/codegen.py`
- `BuildTools/EffekseerEditor/build.ps1`
- `BuildTools/package.py`
- `BuildTools/tests/test_package_include.py`
- `BuildTools/msicreator/createmsi.py`

Important consequences:

- Do not document one game's final target list as universal engine behavior.
- Prefer stage responsibilities and option names over hard-coded generated target names.
- Validate build changes through an embedding project preset whenever possible.

## Apple deployment target

`ThirdParty/iOS-sdk` supplies the minimum iOS deployment version through
`buildtools.py` and the iOS toolchain; it does not select the installed Xcode SDK.
The canonical minimum is `26.0`, matching the effective target that modern Clang
already derives from the obsolete `19.0` version name. LLVM's
[Darwin version alignment](https://github.com/llvm/llvm-project/commit/88f041f3e05e26617856cc096d2e2864dfaa1c7b)
remaps iOS 19 to 26. Using the canonical name avoids the deployment override
diagnostic without changing that effective minimum. Supporting an older iOS
release requires a separate product compatibility decision.

Apple static archives retain guarded translation units and module anchors even
when they contain no symbols. `CMAKE_STATIC_LINKER_FLAGS` passes only
`-no_warning_for_no_symbols` to Xcode's `OTHER_LIBTOOLFLAGS` for static and
object libraries. The iOS toolchain puts the same option directly into its
explicit libtool archive commands for Ninja and Makefiles. Other generators
using `ar` or `llvm-ar` receive no libtool-only option. This diagnostic
policy leaves source inventories intact and does not suppress compiler warnings,
other archive diagnostics, or invalid-input errors. Linux and dynamic-linker
flags are unchanged. `test_apple_archive_diagnostics.py` configures the actual
stage and checks real Mach-O archives with empty and callable members, plus
missing and malformed input failures.

## Stage files

The staged pipeline lives in `BuildTools/cmake/stages/`. Canonical stage order is defined by `BuildTools/Init.cmake`: `Init`, `ProjectOptions`, `ThirdParty`, `EngineSources`, `Codegen`, `CoreLibs`, `Applications`, `ScriptsAndBaking`, `Packages`, `Finalize`.

### `Init.cmake`

Establishes baseline configuration. It declares and checks core project options such as:

- `FO_MAIN_CONFIG`
- `FO_DEV_NAME`
- `FO_NICE_NAME`
- `FO_GEOMETRY`
- `FO_APP_ICON`
- `FO_OUTPUT_PATH`
- build feature toggles such as `FO_BUILD_CLIENT`, `FO_BUILD_SERVER`, `FO_BUILD_MAPPER`, `FO_BUILD_ASCOMPILER`, `FO_BUILD_BAKER`, `FO_UNIT_TESTS`, the scripting toggles, and the independent `FO_SPARK_PARTICLES` / `FO_EFFEKSEER_PARTICLES` particle backends.

Both particle backend options default to `OFF`. An embedding project explicitly
enables either backend or both during a migration. Backend source files remain
in the stable engine source lists and guard their implementation with the
corresponding `FO_*_PARTICLES` macro. A disabled backend contributes no
third-party target, compiled runtime or Mapper implementation, runtime resource
extensions, or baker implementation.

It also establishes build hash and common generation context. Start here when a build option is missing or validated too early/late.

### `ProjectOptions.cmake`

Normalizes and validates project-level option combinations. Examples from the current stage include checks around code coverage, build mode combinations, and scripting/tool compatibility such as `FO_BUILD_ASCOMPILER` requiring AngelScript support.

Start here when a combination of options should be rejected or derived before source lists and targets are created.

### `ThirdParty.cmake`

Adds bundled engine third-party libraries. The stage comment notes that it installs a `find_package()` interceptor before third-party `AddSubdirectory()` calls so vendored libraries cannot silently reach into the host system.

Start here when a bundled dependency is added, removed, or needs build isolation rules.

LibreSSL enables generic assembly only on non-MSVC toolchains; its existing
MSVC x64 path selects `ASM_MASM`. MongoDB's AWS authentication is explicitly
disabled alongside its TLS support, matching the driver's effective feature
set without requesting an authentication mode that requires TLS.

Vendored archive inputs follow their implementations: LibreSSL omits empty
archive fillers, compatibility objects without platform shims, and the generic
AES core when amd64 assembly
provides all of its entry points. Glslang includes its SPIRV-Tools bridge only
with the optimizer enabled. This avoids empty archive members on Apple;
engine-owned source inventories remain unconditional.

MongoDB selects its crypto, TLS, encryption and OS implementation sources from
the enabled features. Its protocol flag/opcode consistency assertions remain
mandatory parts of its RPC and cluster translation units. The checks produce
no standalone empty archive members, including with Xcode's object libraries.
`BuildTools/tests/test_mongoc_archive_inputs.py` checks enabled backend selection,
client/BSON operations, and rejection of deliberately mismatched protocol flags
and opcodes.

`FO_DOTNET_DIR` is a configuration input, so temporary stage-state resets must
preserve its CMake cache value. A nonempty cache override takes precedence over
the environment; otherwise the environment value is used, then the default
`${CMAKE_CURRENT_BINARY_DIR}/dotnet`. Paths containing spaces follow the same
rules. `test_managed_runtime_directory.py` checks the real state initialization
and directory-resolution block without starting a runtime build.

`setup-mono` and CMake's ready marker include the normalized pinned `ThirdParty/dotnet-runtime` revision and
runtime triplet. Changing the pin invalidates both the native build and published runtime. Publication validates
the runtime output, SDK shared-framework directory and Mono core library before replacing the output tree,
so removed files cannot survive a successful republish. Shared-framework versions sort numerically, with a
prerelease ordered before the corresponding release.

The ready-marker command declares the published static archives as CMake `BYPRODUCTS`, using
the same configuration-specific runtime paths as the linker. Ninja requires a file-producing
rule for these archives before it can schedule a clean build; a target dependency on
`SetupManagedRuntime` alone cannot supply that rule. `test_managed_runtime_byproducts.py`
reproduces the missing-rule failure and builds a real shared-library consumer with Ninja and
Ninja Multi-Config in Debug and Release, then verifies that a repeated build reuses the runtime.

Windows targets with managed scripting use the static MSVC runtime (`/MT`, or
`/MTd` for Debug configurations), matching the published Mono and minipal
archives even when no client target is built. Client builds also retain the
static CRT; builds with neither client nor managed scripting retain `/MD` or
`/MDd`. Existing Mono caches already use this contract and need no rebuild.

Nested runtime builds remove Xcode's legacy `TARGETNAME` environment variable before invoking
MSBuild. MSBuild treats environment property names case-insensitively and otherwise names every
task and generator output `SetupManagedRuntime.dll`, causing duplicate publish files and failed
generator loads. The BuildTools
regression publishes two actual SDK projects under a poisoned target name and verifies distinct
assemblies; successful runtime caches retain their identity.

For iOS device and simulator runtimes, the nested build also removes inherited
`SDKROOT`. Mono supplies the target SDK in its CMake arguments, while its cross-AOT
compiler runs on macOS and must discover the macOS SDK. Otherwise CMake initializes
the host compiler's sysroot from Xcode's iOS environment and rejects host APIs such
as `system()`. `DEVELOPER_DIR` and other toolchain inputs remain available; ordinary
macOS runtime builds retain an explicitly selected `SDKROOT`. The regression runs
actual Darwin/iOS CMake configuration with SDK-discovery fixtures, checks the wrong
host sysroot before isolation, and verifies both the corrected host SDK and the
unchanged target SDK. It does not replace a managed Apple build with installed SDKs.
Retry a failed build from this environment error with a fresh runtime object tree:
an existing CMake cache retains its previously selected SDK even after the parent
environment is corrected. Successful runtime caches remain valid.

On Windows, the nested runtime also removes the outer generator's `INCLUDE`, `LIB`, and `LIBPATH`.
The runtime initializes its own host toolchain, while a pinned outer toolset can advertise optional
ATL/MFC directories that are not installed. Roslyn rejects those missing search paths before the
runtime reaches its own Visual Studio initialization. The environment regression poisons all three
variables and verifies that unrelated host settings still reach the runtime wrapper.

Managed Apple targets link Foundation, CoreFoundation, and the Objective-C runtime
through the common managed dependency set. These dependencies belong to Mono and
its static native shims, including headless targets that do not link SDL. Runtime
archives use full paths on Apple: Xcode otherwise adds a configuration subdirectory
to library search paths, although Mono publishes directly under the triplet's
`lib` directory. The regression builds and links actual Mach-O shared libraries for
macOS arm64/x64, iOS arm64, and the x64 simulator with SDK symbol fixtures; installed
Apple SDK builds remain the platform acceptance check.

iOS device and simulator targets also link `icucore`: their hybrid globalization
shim calls ICU directly, so the system library must follow the published static
archive into the final executable. macOS uses the runtime's dynamic ICU lookup.
The Apple link regression includes an iOS ICU symbol and verifies that removing
the dependency makes the actual Mach-O link fail.

Source-built Apple runtimes receive narrowly anchored patches to the pinned Mono
sources before compilation. JIT-only locals and tables follow the existing JIT
guards; fixed EventPipe array bounds use C integer constant expressions. Native
PAL conversions are explicit and cleanup jumps do not cross initialized locals.
The `getdomainname` configure probe checks the function's parameter type with a
compile-time array bound: a mismatch is a compiler error even if warning
diagnostics are disabled or demoted. Its separate CMake cache result also replaces
values produced by the former warning-based probe. CMake policies CMP0156 and CMP0179, when available,
deduplicate static archives for linkers that support rescanning them. Diagnostics
remain enabled. These patches preserve the published runtime layout and are
idempotent; an unexpected upstream source shape stops setup for review.

iOS source builds preserve signed collation option masks when calling the native
helpers and initialize the sendfile fallback's buffer bound before cleanup jumps.
Vector I/O checks API availability at runtime before using `preadv` and `pwritev`
(iOS 14 or newer); older supported systems use the existing `pread`/`pwrite`
loops. This preserves the deployment minimum, positional offsets, partial I/O,
and interrupted-call retry behavior. The regression compiles availability
annotations for device and simulator targets, then executes both paths with
real file I/O on the host. The `_apple_sources_v2` cache marker covers
these corrections together with the common Apple source patches, and forces one
rebuild and republication of runtimes with the former signature probe. Setup
upgrades an already patched source checkout without recloning it; subsequent
invocations reuse the corrected runtime. Other platforms keep their cache keys.

Android source builds also match the native elliptic-curve diagnostic's variadic
format to an explicit unsigned enum conversion. The source patch preserves the
reported curve values and keeps format diagnostics enabled. Its regression
compiles the diagnostic for all three Android architectures and executes it on
the host, including rejection of ambiguous or changed upstream source anchors.

Android x86 Mono builds use `lock cmpxchg8b` for 64-bit atomics and derive the
other operations from CAS observations. This preserves the ABI's four-byte
alignment of `gint64`, including fields exposed through managed `Interlocked`,
without assuming eight-byte alignment or introducing a library mutex that could
deadlock during GC suspension. The locked instruction and compiler memory
clobber preserve Mono's full ordering; other architectures retain their upstream
implementation. Regression coverage compiles the Android x86 PIC path and, when
a Linux i386 loader and libc are installed, executes concurrent operations at
both four- and eight-byte alignment.

Android and Apple source patches have separate `BUILT` and `READY` marker
suffixes, synchronized between `buildtools.py` and the CMake runtime target.
Existing caches with the older suffix rebuild and republish those runtimes once;
the cloned source is retained. Linux, Windows, and browser marker keys stay
unchanged. Change the affected platform's suffix when its patch contract changes,
so a ready cache cannot bypass new source edits.

Runtime source builds also set `UseSharedCompilation=false`. A shared Roslyn server can retain an
interop generator's dependency path from a completed runtime checkout. Deleting that checkout then
makes another build fail with CS8784 even though its own `Microsoft.Interop.SourceGeneration.dll`
exists. A private compiler loads the current checkout's generator companions, so completed
workspaces can be cleaned without invalidating another build's analyzer context.
Windows passes the property with MSBuild's `/p:` spelling through `build.cmd` and
PowerShell; `-p:` is ambiguous with the runtime script's named parameters. Unix
builds retain `-p:`. The regression exercises PowerShell parameter binding with
the conflicting runtime parameter names and preserves unrelated properties.

Before each runtime source build, `setup-mono` patches the runtime's zlib-ng
target to remove Mono's inherited MSVC `/W4` option. Mono keeps `/W4`, while
zlib-ng retains its own `/W3`, additional diagnostics and `/WX`; this prevents
conflicting warning-level options without suppressing diagnostics. The patch
also applies when rebuilding an existing clone. Already built or published
runtime caches remain valid because the effective warning level and binary
behavior are unchanged.

The managed setup command calls the absolute host Python interpreter selected
by CMake (Python 3.11 or newer). It retains that interpreter when a build tool
changes `PATH`, as Xcode does for script phases. `Python3_EXECUTABLE` can select
an explicit interpreter at configure time; the standalone `setup-mono` wrappers
remain convenience entry points for an interactive shell.

### `EngineSources.cmake`

Builds source lists and generated resource files used by later stages. It appends source lists for engine layers such as Essentials, Common, Frontend, Client, Server, Tools, Scripting, and tests. It also prepares app icon/resource data such as the generated Windows `.rc` file.

Start here when a new hand-authored source file must become part of a core engine library.

`AddEngineSource(COMMON ...)` also forwards contributed `.h` files to codegen.
Header classification requires the literal `.h` suffix; its bracketed-dot regex
retains that meaning across CMake macro argument policies without backslash
re-interpretation.

### `Codegen.cmake`

Constructs the code-generation command and output set. It passes project and engine metadata to `BuildTools/codegen.py`, including main config, build hash, generated output path, project names, embedded data capacity, metadata source files, and added common headers.

It creates codegen targets such as normal and forced code generation. Start here when generated C++/script API metadata changes.

Related doc: [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).

### `CoreLibs.cmake`

Creates core static libraries from the source lists prepared by `EngineSources.cmake`. Current responsibilities include libraries such as Essentials, Common, frontend/headless app layers, scripting integration libraries, client/server libraries, baker libraries, and testing support depending on enabled options.

`EngineSources.cmake` includes the native `EffekseerCompiler.h/.cpp` module in
`BakerLib`. With Effekseer particles enabled, `ParticleBaker` calls it directly
to compile fixed Editor-1.80.5 `.efkproj` XML and obtain each project's
referenced-resource list for the per-effect path/size/write-time snapshot under
`BakeOutput/.baker-cache`. Runtime libraries and Web clients do not depend on a
compiler target or host process; they consume pre-baked `.efk`. A server-only
build no longer enables BakerLib merely because `FO_BUILD_SERVER` is set.

Start here when source grouping, library dependencies, or runtime layer boundaries change.

### `ScriptsAndBaking.cmake`

Creates custom targets for script compilation and resource baking. Current responsibilities include:

- AngelScript compilation through the project AS compiler target when AngelScript scripting is enabled.
- Managed script generation and compilation through the `ManagedScriptBakerApp` (`<FO_DEV_NAME>_ManagedScriptBaker`, wired to the `CompileManagedScripts` target) when Managed scripting is enabled, with `SetupManagedRuntime` preparing Mono, Mono corelib, and the managed .NET class libraries under the CMake build tree before managed-linked applications are built. `PrepareManagedRuntimePayload` filters that publish tree down to managed PE assemblies from `lib/netcoreapp`, requires `System.Private.CoreLib.dll`, and writes a SHA-256 manifest. The Managed baker places this clean payload under `ManagedRuntime/` in its resource pack; native Mono/JIT files, headers, import libraries, symbols, and other build products never enter the resource output. Setup also builds and publishes the interop shims (`libs.native`) and `libminipal`, which CoreLib reaches the OS through on every non-Windows platform — `Interop.Sys` is `libSystem.Native`, and the first managed call already needs it. The shims are linked statically and resolved at run time from a generated entry-point table (`BuildTools/generate_pinvoke_table.py`) served through a Mono dl fallback, because Windows and WebAssembly cannot load them as shared libraries. For the browser the subset additionally carries `mono.wasmruntime`, whose JavaScript glue is published beside the runtime and passed to the Emscripten link as `--pre-js` / `--js-library` / `--extern-post-js`.
  The runtime is built with the **host's** toolchain, so `SetupManagedRuntime` follows the host and not the target: CMake invokes `buildtools.py setup-mono` with its configured Python interpreter; that helper invokes the runtime's `build.cmd` on Windows and `build.sh` elsewhere. One target is out of reach that way — `dotnet/runtime` has no Windows cross-target, so a non-Windows host cannot produce `windows.<arch>.<config>` at all. For that case the runtime is built once on Windows and handed over: point **`FO_MANAGED_RUNTIME_PREBUILT`** at a directory holding published `output/mono/<triplet>` trees (or at a single triplet's tree) and `setup-mono` adopts it in place of the source build, writing the same ready marker. Without it, a Windows target on a non-Windows host is refused at configure time with the reason rather than failing later inside `dotnet/runtime`.
- Resource baking through the project baker target.
- Build-hash/write-hash support for baked resources.
- Normal and forced bake targets.

The stage exposes `AddBakingTarget(<target> [SUB_CONFIG <name>] [FORCE]
[COMMENT <text>])` for embedding-project variants. Call it directly after
`SetupScriptsAndBaking()` instead of wrapping one declaration in a project
function:

```cmake
SetupScriptsAndBaking()
AddBakingTarget(BakePublicResources
    SUB_CONFIG PublicGame
    COMMENT "Bake public resources")
```

The helper is available after the stage has defined the standard baking targets.

Related docs: [BakingPipeline.md](BakingPipeline.md) and [Scripting.md](Scripting.md).

### `Applications.cmake`

Creates executable and shared-library applications from `Source/Applications/*.cpp`. It uses helpers such as `AddExecutableApplication` and `AddSharedApplication` and project variables such as `FO_DEV_NAME`, output paths, platform flags, and enabled build modes.

Examples of entry points wired here include client, client runtime library, client headless variants, server variants, mapper/editor/tool apps, baker, AngelScript compiler, and testing app depending on options.

Effekseer Editor is intentionally absent from this stage and from the
application target graph. Its standalone `BuildTools/EffekseerEditor/build.ps1`
entry point configures and builds upstream sources independently of an
embedding project's FOnline CMake configuration. It reads CMake capabilities
and supplies `CMAKE_POLICY_VERSION_MINIMUM` only with CMake 4 or newer, where
that option is supported.

See [Applications.md](Applications.md).

### `Packages.cmake`

Creates package targets from `FO_PACKAGES` and calls `BuildTools/package.py` with project context such as main config, build hash, developer name, nice name, input/output paths, platform/architecture/config data, and binary-output postfix.

`package.py` owns the reusable package payload layout and optional post-processing. For a Windows Client package that includes the `Wix` pack, it invokes `msicreator/createmsi.py` to build an MSI after the Raw payload is staged: the MSI gets the temporary `INSTALLED` marker used by installed-client writable-path resolution, registers the deep-link URI scheme, creates Start Menu + Desktop shortcuts and an Add/Remove Programs icon, and always presents an editable installation-directory dialog. The same inline dialog authoring is used by both supported build hosts; it does not disappear when production runs `wixl` on Linux. The MSI is a **required** artifact when the `Wix` pack is requested — a missing toolset (`wixl` 0.102 or newer on POSIX hosts, with its bundled `ui` extension; WiX `candle`/`light` on Windows) or a generator/build error fails the package (it is not a silent best-effort step). On Debian/Ubuntu, `wixl` ships in its own `wixl` apt package, not in `msitools`. All installer values are read from the embedding project's config, so the packager stays game-agnostic:

- product/manufacturer/comments name ← `Common.GameName` (falls back to the package nice name)
- `ProductVersion` ← `Common.GameVersion`, with `$FILE{...}` indirection resolved relative to the main config directory (so a `$FILE{VERSION}` setting yields the real numeric version, not a `0.0.0` fallback)
- deep-link URI scheme ← `Auth.UriScheme`
- stable WiX `UpgradeCode` ← `Packaging.MsiUpgradeCode` (required; must never change once an MSI has shipped)
- Add/Remove Programs icon ← `Packaging.AppIcon` (optional)
- install directory name and MSI base name ← the package nice name

An explicit `INSTALLDIR` passed to `msiexec` has highest priority. Otherwise, a first-time interactive install prefers the path remembered by an earlier MSI, then the per-user writable `%LOCALAPPDATA%\<Common.GameName>` fallback. The selected path is stored under `HKCU\Software\<nice-name>\InstallLocation` and the directory screen always permits direct editing or browsing, including an explicit `Program Files` choice. The standalone MSI does not inspect or target Steam or another store's installation infrastructure.

An MSI upgrade deliberately keeps a remembered `Program Files` path instead of moving an existing tree. The refreshed `INSTALLED` marker still routes cache, logs, resources, and native runtime updates to the per-user writable overlay described in [ClientUpdater.md](ClientUpdater.md), so the retained executable location does not block later self-updates.

The portable Raw/Zip artifacts are finalized before the MSI step and never carry the `INSTALLED` marker, so they stay portable.

Native client hosts and runtime libraries share a platform/architecture binary
directory. The packager copies ordinary runtime dependencies from that directory,
but treats every engine-owned `Client`/`ClientLib` and
`ClientHeadless`/`ClientLibHeadless` library name as an application binary rather
than a companion dependency. It then adds only each explicitly requested client
variant under its packaged basename. Consequently a stale or separately built
headless runtime remains available as a build artifact without leaking into a
normal Raw/Zip payload or the MSI derived from it; a package carrying the
`Headless` token still receives the renamed headless host/runtime pair.

Managed class libraries are not binary companions. The Managed baker has already
written the filtered payload into the managed resource pack, so `package.py` does
not copy a side-by-side `ManagedRuntime` directory and does not hoist Mono DLLs
into a package root. Native, Web, and Android packages all deliver the same
resource-pack paths; the backend restores them into the writable runtime cache.

When several package parts append to one `SingleZip`, byte-identical files at
the same archive path are coalesced into one entry. Different contents at the
same path are a packaging error; the packager never emits ambiguous duplicate
ZIP names. Applications sharing a package root must therefore agree on every
common file they emit. Managed class libraries do not collide there because they
live once in the shared resource pack. `buildtools.py build <platform> full
<config>` builds the client, server and tools in one CMake tree with one
`SetupManagedRuntime` output. Both `full` and `toolset` leave
`FO_BUILD_ASCOMPILER` to the embedding project's default so a managed-only
project does not enable AngelScript tools.

The universal package schema has no `EffekseerEditor` binary role. Separately
built tools are declared alongside `BINARY` parts with
`INCLUDE <source-path-glob> <target-path-in-pack>`. The source glob is relative
to `FO_OUTPUT_PATH`. After the ordinary binary parts are assembled, the generic
packager replaces the included target tree and updates an existing `SingleZip`
at `<output>/<devname>-<package>/<devname>-<package>.zip`, alongside the staged
package payload, without duplicate or stale entries. This path is covered by
`BuildTools/tests/test_package_include.py`.

Start here when platform package layout, package target naming, package script arguments, or package-time installer metadata changes.

### `Finalize.cmake`

Performs final solution/project organization and late reporting. Current responsibilities include target folder grouping, optional ReSharper settings copy, third-party dummy grouping, and verbose cache-variable reporting when `FO_VERBOSE_BUILD` is enabled.

Start here for final target organization or post-generation diagnostics, not for source ownership or build feature validation.

## Helper files

Reusable helpers live in `BuildTools/cmake/helpers/`:

- `Build.cmake` — build/target creation helpers.
- `Commands.cmake` — command target helpers.
- `Options.cmake` — option/value helpers.
- `State.cmake` — staged pipeline state/hook support.
- `WriteBuildHash.cmake` — writes build-hash state used by generation/baking flows.

When a stage needs reusable behavior, prefer adding a helper here instead of copy-pasting logic between stages.

## Stage hooks

Stage comments reference the hook convention:

```cmake
AddStageHook(<StageName> Pre|Post <macro-name>)
```

Use hooks when an embedding project or a later refactor needs to extend stage behavior without editing the middle of a stage body. Keep hook behavior documented near the owning stage or in the project docs if it is game-specific.

## Change routing

- New project option or option validation: `Init.cmake` / `ProjectOptions.cmake`.
- New vendored dependency: `ThirdParty.cmake`.
- New engine source file: `EngineSources.cmake` and maybe `CoreLibs.cmake`.
- New generated metadata/API behavior: `Codegen.cmake` and [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md).
- New script compile or resource bake behavior: `ScriptsAndBaking.cmake`, [BakingPipeline.md](BakingPipeline.md), and [Scripting.md](Scripting.md).
- New executable/tool entry point: `Applications.cmake` and [Applications.md](Applications.md).
- Auxiliary-tool build recipes: `BuildTools/buildtools.py build-auxiliary`,
  `BuildTools/EffekseerEditor/build.ps1`, and [Tools.md](Tools.md).
- New package layout or installer metadata: `Packages.cmake`, `BuildTools/package.py`, `BuildTools/msicreator/createmsi.py`, plus platform docs.
- Final target organization or verbose diagnostics: `Finalize.cmake`.

## Validation checklist

For BuildTools changes:

1. Configure from a real embedding project root.
2. Use the narrowest preset that exercises the changed stage.
3. For source-list changes, verify the affected target builds.
4. For codegen changes, verify generated files and script API consumers.
5. For baking changes, run normal and forced bake paths when relevant.
6. For Effekseer Editor changes, run `buildtools.py build-auxiliary
   effekseer-editor Release` on Windows win64 and inspect the staged
   managed/native/resources payload; exercise the package `INCLUDE` when the
   developer-package layout changes.
7. For package changes, run the affected package target and inspect output layout; for WiX/MSI changes, also verify the generated installer config/registry values or run the installer build on a host with WiX/wixl.
8. Run documentation link checks if docs changed.
9. Run `git diff --check` before reporting completion.
