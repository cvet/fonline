# Testing

> Engine-owned documentation. This page maps the current engine test executable, generated test targets, coverage targets, and every `Source/Tests/Test_*.cpp` suite currently present in the checkout.

## Purpose

Use this page when choosing validation for an engine change or when adding/removing tests. The source-tree README at [../Source/Tests/README.md](../Source/Tests/README.md) is a short entry point; this page is the maintained full test map.

## Source paths inspected

- `Source/Applications/TestingApp.cpp`
- `Source/Tests/README.md`
- all current `Source/Tests/Test_*.cpp` files
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/codecoverage.py`
- `BuildTools/validate.sh`
- `BuildTools/validate.cmd`
- parent VS Code task references where available

## Test runner model

`Source/Applications/TestingApp.cpp` is the test application entry point. It requires `FO_TESTING_APP`, initializes the application layer with `InitApp(-1, nullptr)`, marks `IsTestingInProgress`, and delegates execution to `Catch::Session().run(argc, argv)`.

`BuildTools/cmake/stages/EngineSources.cmake` owns `FO_TESTS_SOURCE`, the explicit list of test source files compiled into test builds. `BuildTools/cmake/stages/Applications.cmake` builds test executables through `SetupTestBuild(name)`:

`BuildTools/check_windows7_imports.py <binary> [...]` is a standalone PE-level regression check for Windows 7 artifacts. It rejects the reported `CreateFile2` import; embedding-project CI should run it after linking and before packaging.

- `UnitTests` when `FO_UNIT_TESTS` is enabled;
- `CodeCoverage` when `FO_CODE_COVERAGE` is enabled.

For an embedding project with dev name `LF`, the standard generated names are `LF_UnitTests`, `RunUnitTests`, `LF_CodeCoverage`, `RunCodeCoverage`, `GenerateCodeCoverageReport`, and `AnalyzeCodeCoverage`. Treat the prefix as project-generated, not universal.

## Running tests

Preferred local baseline from a configured build:

```bash
cmake --build . --config RelWithDebInfo --target RunUnitTests
```

With `FO_EFFEKSEER_PARTICLES` enabled, the focused `[particle]` Catch2 cases
invoke the published helper through the production `ParticleBaker` path. They
cover text compilation, dependency invalidation, malformed XML, and rejection
of cooked files presented as authored inputs.

The executable target can also be invoked directly when you need Catch2 arguments. In Last Frontier-style layouts, test binaries are emitted under `Binaries/Tests-*`, for example `Binaries/Tests-Windows-win64/LF_UnitTests.exe` or `Binaries/Tests-Linux-x64/LF_UnitTests`.

With Visual Studio/MSBuild generators, `RunUnitTests` writes the test process output to `<build-dir>/<ProjectDevName>_UnitTests.log` and uses the test process exit code as the pass/fail signal. This keeps expected negative-case diagnostics such as compiler `error` lines from being reclassified as MSBuild errors. When the run fails, the helper also echoes the captured output before failing, so a failure is diagnosable from the build output alone — on CI the log file never leaves the runner, and the exit code by itself does not say which test or assertion broke.

For broad validation scenarios, the BuildTools validators can run selected scenarios:

```bash
Engine/BuildTools/validate.sh unit-tests
Engine/BuildTools/validate.sh android-arm64-client linux-client linux-server
```

Use the smallest focused tests first, then the broader run target when the change crosses subsystem boundaries.

### Unit tests under sanitizers

The unit tests also run under Clang sanitizers via dedicated validators, which select
the matching `San_*` build type and run `RunUnitTests` instrumented:

```bash
Engine/BuildTools/validate.sh unit-tests-san-address    # AddressSanitizer (+LeakSanitizer)
Engine/BuildTools/validate.sh unit-tests-san-memory     # MemorySanitizer (requires Workspace/msan-libcxx)
Engine/BuildTools/validate.sh unit-tests-san-undefined  # UndefinedBehaviorSanitizer
Engine/BuildTools/validate.sh unit-tests-san-thread     # ThreadSanitizer
```

The `validate.yml` workflow runs these as a `unit-tests-sanitizers` matrix job.
ASan/MSan/UBSan/TSan are blocking legs. The `unit-tests-san-memory` validator prepares
`Workspace/msan-libcxx` by building LLVM's `libc++`, `libc++abi`, and `libunwind`
with MSan instrumentation, then configures `San_Memory` with `FO_MSAN_LIBCXX_ROOT`.
The runtime build applies a narrow libunwind ignorelist so C++ exception and
sanitizer-report unwinding do not self-report on ABI register snapshots. Engine
native stack capture and the backward-cpp signal handler are disabled under MSan and
TSan so the sanitizer runtimes own their reports; backward-cpp/libbfd symbolization
under TSan also produces prohibitive shadow-memory growth. `unit-tests-san-memory-with-origins`
is available locally as the slower diagnostic variant when a future MSan finding
needs origin tracking. `San_DataFlow` remains
intentionally unwired: DataFlowSanitizer is a taint-tracking framework, not a
defect detector.

Applications that load `BakerLib` while running under a sanitizer must use a baker
built with the same `San_*` configuration. Hiding the plugin's ELF exports prevents
direct symbol interposition, but calls implemented inside the shared C++ runtime may
still allocate through the host and return to an inline deallocator in the plugin.
Matching configurations keep the sanitizer runtime and allocator contract identical
on both sides of that module boundary.

On MSVC, the `San_Address`/`Debug_San_Address` configs additionally link executables with
`/STACK:8388608` (`AddExecutableApplication` in `BuildTools/cmake/helpers/Build.cmake`):
ASan's stack-frame inflation overflows the 1 MiB Windows executable default on recursion
depths that fit every production configuration, so sanitizer runs get the same 8 MiB
reserve that Linux runs already have from the default rlimit. Production configs keep the
1 MiB default.

Vendored third-party libraries are excluded from UBSan's `-fsanitize=function` and
`-fsanitize=alignment` checks (the rest of `-fsanitize=undefined` still applies to them).
`DisableLibWarnings` adds `-fno-sanitize=function,alignment` on the
`San_Undefined`/`San_Address_Undefined` configs because several vendored libraries trip
those two checks by design:

- `function`: AngelScript's script-call dispatch invokes registered C functions through
  `bool(*)(void*,void*)` and similar signatures, and C callback APIs do the same.
- `alignment`: AngelScript builds its bytecode in an `asDWORD[]` (4-byte) buffer and packs
  pointer-sized `asPWORD` operands at 4-byte-aligned slots
  (`*(asPWORD*)(bc+1) = ...` in `GenerateFactoryStubForTemplateObjectInstance`), which UBSan
  reports as a misaligned store even though it is correct on every architecture the engine
  targets.

Both are third-party idioms, not undefined behaviour in engine code, so they must not fail
the UBSan leg (which CI runs with `halt_on_error=1`). First-party engine code keeps both
checks fully active.

LeakSanitizer runs as part of the address-sanitizer leg (CI sets `ASAN_OPTIONS=detect_leaks=1`).
It runs with **no suppression list** — every leak it can report is fixed at the source rather than
masked. Notable cases:

- backward-cpp's libbfd stack-trace resolver (`Source/Essentials/StackTrace.cpp`) caches each
  binary's ELF symbol table and DWARF debug info inside libbfd, hung off the open `bfd` handle, and
  never fully frees it on `bfd_close`. The resolver is therefore a single process-lifetime instance
  (`get_native_trace_resolver`, serialized by `stack_trace_state::native_resolver_locker`): it is created
  once, never destroyed, and stays reachable from a static root, so each binary is symbolized once
  and those libbfd caches remain reachable — LSan does not report them.
- The AngelScript backend deletes the preprocessor line-number translator during engine userdata
  cleanup, and each SPARK context frees its `IOManager` converters at context shutdown.
- Owning containers free their contents transitively: e.g. `EntityTypeDesc::PropRegistrar` is a
  `unique_ptr` so every `PropertyRegistrar` (and the `Property` objects it holds) is freed when
  `EngineMetadata`'s type maps are destroyed.

## Code coverage

When `FO_CODE_COVERAGE` is enabled, `BuildTools/cmake/stages/Init.cmake` selects the backend from the compiler:

- MSVC / clang-cl: MSVC-style coverage output;
- Clang: LLVM profile/coverage mapping;
- GCC: GCC/lcov-style coverage flags.

`BuildTools/cmake/stages/Applications.cmake` wires coverage command targets through `BuildTools/codecoverage.py`:

- `CleanCodeCoverageData`
- `RunCodeCoverage`
- `GenerateCodeCoverageReport`
- `AnalyzeCodeCoverage`

Coverage output is rooted under `CodeCoverage/<Toolchain>/<Platform-Config>/`.
`BuildTools/codecoverage.py` reports first-party production engine sources under
`Engine/Source/`; it excludes `Source/Tests/`, `ThirdParty/`,
`GeneratedSource/`, and `Applications/` from the denominator. See
[../Source/Tests/README.md](../Source/Tests/README.md) for current local task
notes.

Coverage is a per-platform, per-environment measurement, and the denominator
reflects that in two different ways:

- Sources **not compiled** into the current build produce no coverage mapping
  and are reported separately as untouched. Direct3D rendering drops out of a
  Linux run this way with no configuration; measure it on a Windows run.
- Sources that **compile here but cannot execute in a headless test process**
  are listed in `ENVIRONMENT_EXCLUDED_SOURCES`, each with a written reason —
  currently the device-backed audio/video paths, Mongo/updater infrastructure,
  and the deliberately process-killing diagnostic self-test. Loopback sockets
  and the debugger endpoint are exercised headlessly and stay in the headline.

The summary prints the scoped headline, a combined all-sources figure, and the
excluded bucket file-by-file with reasons, so the split stays auditable. Adding
an entry there is a routing decision, not a write-off: it must be covered by the
layer that can run it — a windowed/rendering run on the owning platform, or an
integration suite with real endpoints.

### Covering ImGui diagnostic panels

`DrawGui()` implementations normally only run inside the windowed application,
but they are reachable from unit tests through a backend-less ImGui context: no
renderer is attached and the draw data is discarded, while every panel builder
runs for real. `Test_ServerEngine.cpp` shows the pattern. These details matter:

- Declare `ImGuiBackendFlags_RendererHasTextures` on the IO. The legacy
  `GetTexDataAsRGBA32` / `GetTexDataAsAlpha8` atlas-upload entry points are
  compiled out by `IMGUI_DISABLE_OBSOLETE_FUNCTIONS`, so letting ImGui own the
  atlas is the only way to satisfy the "font atlas is not built" check in
  `NewFrame()`.
- A collapsed `TreeNode` skips its body, so a plain frame only covers the
  outermost level. Force-opening the stored state does not help: ImGui writes a
  node's open state only once something opens it, so a node that was never
  clicked has no `StateStorage` entry to flip. Wrap the draw call in
  `ImGui::LogToBuffer(depth)` / `ImGui::LogFinish()` instead — auto-expanding tree
  nodes is a documented side effect of logging. Collapsing headers carry
  `ImGuiTreeNodeFlags_NoAutoOpenOnLog` and opt out, so seed their ids into the
  window's `StateStorage` by hand.
- Logging also captures the rendered text, which is the cheapest way to prove
  the walk really descended instead of rendering a row of closed headers. Read
  the buffer before `LogFinish()`, which clears it, and assert on markers from
  the nested panels so a renamed panel fails the test rather than silently
  dropping coverage.
- Server diagnostics run at an engine sync point, but that does not implicitly
  cover entity state. `ServerEngine::DrawGui()` snapshots not-logged-in players
  because they are intentionally absent from the entity registry, then acquires
  one replacement cover for that snapshot plus the registered world. The
  snapshot is taken under the publication lock and that lock is released before
  any entity lock, because `OnPlayerConnected` takes the two in the opposite
  order. `EnsureEntitySynced` is not an alternative here: it only retains an
  entity the context already covers and throws for one it does not, which is
  exactly the case for a player outside the registry. Tests should keep a real
  not-logged-in connection in the snapshot so this boundary cannot regress.
- **Pass an explicit depth to `LogToBuffer`.** The default auto-open depth is 2,
  so anything nested deeper stays collapsed and its body never runs. Raising it
  (`ImGui::LogToBuffer(12)`) took the SPARK particle editor from 27% to 48%
  without a single new assertion, because its object inspector is a deep
  reflective tree.
- **Collapsing headers need their state seeded by hand** even with logging on:
  they opt out of log auto-expansion, so a panel whose body lives under one never
  renders. Seed with
  `window->StateStorage.SetInt(ImGui::GetID(id), 1)` inside the enclosing
  `Begin()` before drawing.
- Destroy the context at scope exit — other tests assert that no ImGui context
  leaks between test cases.

### Pressing a widget so the branch behind it runs

Drawing a panel covers its layout, not its behaviour: the body of every button,
checkbox, selectable and tree node stays unreachable because nothing is ever
clicked. `Test_ImGuiHarness.h` closes that gap, and `Test_ImGui.cpp` pins the
harness itself against a window the test owns. The rules that matter:

- `ImGuiTestHarness::ActivateItem(window, label)` queues an activation for the
  widget that `label` builds inside `window`. ImGui consumes it when it meets the
  widget again, so a press needs **two frames**: one to submit the widget and one
  to run the branch behind it.
- **Draw only the panel that owns the control.** A neighbouring window that calls
  `ImGui::SetKeyboardFocusHere()` while it appears queues a focus move, and
  `NavMoveRequestApplyResult()` overwrites the pending activation before the
  target widget is ever reached — the press silently disappears. The mapper's
  Map Browser does exactly this, which is why the mapper control test draws one
  panel per press instead of the whole editor.
- A code activation leaves the pressed widget owning `ActiveId` with no input
  source that would ever release it, so the harness clears the active id before
  queueing the next press. Without that, only the first press of a run lands.
- Controls laid out inside `ImGui::BeginChild` belong to the child's id stack.
  Address them with `ActivateChildItem(window, {child, …}, label)`, which rebuilds
  the `"<parent>/<label>_<id>"` name ImGui gives a child window and walks a chain
  of them for nested children.
- `SetItemOpen(window, label)` seeds a collapsing header, tree node or menu open;
  `SetWindowCollapsed` folds the window itself.
- A press that runs but changes nothing observable usually means the fixture
  disabled the feature rather than that the press was lost — the mapper zoom
  buttons are no-ops until `MapZoomEnabled` is overridden on, because the
  headless direct-draw path turns map zoom off.

### What an inbound remote call can reach

The handler is entered with the calling player covered, plus - transitively - the critter it controls.
Everything else needs an explicit `Game.Sync(...)`, and some native paths reach further than any cover a
script can prepare. Reachable on a *second* critter after `Game.Sync(npc)`: `Map.AddCritter`,
`Critter.SetDir`, `Critter.Action` and synchronized property writes. Not reachable on a critter the caller
does not control: `TransferToHex`, `SetCondition`, `DestroyItem`, `AttachToCritter` and
`Game.DestroyCritter`, plus moving a map item into an inventory and reusing one location across two logins.

A script-level `catch` around a failing call does **not** contain the damage: the sync violation still
tears the session down, so the next remote call never arrives. A test that probes these operations behind
`try`/`catch` therefore reports "the last step was never sent" rather than the operation that actually
failed - drive only what is reachable.

### Covering the crash reporter

`ExceptionHandling.cpp` publishes `SetCrashStackTrace`, `SetCrashSignalInfo`,
`SetCrashSehInfo`, `SetCrashTerminationInfo` and `GetCrashStream` to
`backward.hpp` only — they carry no engine namespace and appear in no engine
header, so a test declares them exactly as that header does. The report is
emitted through the base log on the first write to the crash stream, so point
`logging::to_file` at a private file, write one line into `GetCrashStream()` and read
the report back instead of letting "FATAL ERROR!" leak into the test console.
Restore the log with `logging::to_file("/dev/null")` (`"NUL"` on Windows); there is no
"stop logging to a file" call. Terminating reporters are covered out of process
through `DiagnosticSelfTest`: `main_strong_assert` covers `exceptions::report_and_exit`,
`main_basic_strong_assert` and `main_fatal_exit` cover the early `FatalError`
layer, and `main_failure_exit` pins the raw status-only `exit_app(false)` contract.
The embedding project's
`Tools/PipelineTests/test_crash_diagnostics_linux.py` asserts their log and exit
contracts without killing the unit-test process.

### Covering the text formatter without a real font asset

`FontManager` refuses to answer any metric for an unbound slot, so text
measurement, wrapping and drawing are unreachable until a font exists. Both
loader formats can be synthesized in-memory, which is cheaper and more stable
than shipping a binary asset:

- The `.fofnt` path is a plain text descriptor. `Version 2`, an `Image` line
  naming a sprite that the same data source provides, `LineHeight` / `YAdvance`,
  then one `Letter '<ch>'` block per glyph with `PositionX/Y`, `Width`,
  `Height`, `OffsetX/Y` and `XAdvance`, terminated by `End`. Every glyph may
  point at the same cell — the formatter only reads the metrics.
- The `.fnt` path is BMFont binary: the `BMF\3` signature followed by the info,
  common, pages and chars blocks. Each block is a type byte, a `uint32` length
  and the payload; the loader validates that the info block's four padding bytes
  read as `1/1/1/1` and that the common block declares exactly one page, and it
  reads 20 bytes per glyph from the chars block.

Bind with a scale in `(0..1]` — larger scales are rejected on purpose, because
the intended fix for bigger text is a bigger font asset.

`SplitLines` paginates into rect-sized pages rather than into individual lines:
it emits an entry only once the text overflows the rect height, so a test that
wants several entries needs a short rect, not merely embedded newlines.

### Driving a logged-in client↔server session

A connected client is not a logged-in one: the pre-login session accepts only a
remote call, so login is script-driven from the client. The pieces that have to
line up:

- Declare the login call in both metadata blobs with opposite directions —
  `"In"` on the server, `"Out"` on the client. The `SubsystemHint` token is the
  owning script **file**, and its stem is the namespace the inbound handler is
  looked up in.
- The inbound handler is `void <namespace>::<CallName>(Player player, args...)`
  and must carry `[[ServerRemoteCall]]`.
- The client invokes it as `CurPlayer.ServerCall.<CallName>(...)`; the server
  invokes client-bound calls as `player.ClientCall` / `critter.PlayerClientCall`.
- The reverse direction is symmetric: declare the call `"Out"` in the server blob
  and `"In"` in the client blob, and give the client handler `[[ClientRemoteCall]]`.
  Its shape is `void <namespace>::<CallName>(args...)` — no player argument,
  which is the only difference from the server-side handler.
- Login inserts the player document **before** `OnPlayerLogin` fires, and the
  database refuses an empty document, while every engine-owned `Player` property
  is read-only from script. A test fixture therefore has to declare its own
  persistent player property and set it in the handler.
- To reach the world-entry protocol (load map, add critter, initial property
  sync), the handler continues with `Game.CreateCritter(pid, true)` →
  `player.SwitchCritter(cr)` → `Game.CreateLocation(pid, mapPids)` →
  `cr.TransferToMap(map, hex)`.
- Both static map resources open with the format header (`BAKED_MAP_FILE_MAGIC`,
  `BAKED_MAP_FILE_VERSION`); a blob without it is rejected before anything else
  is read. After the header the layouts differ: `.fomap-bin-server` carries three
  counts (hashes, items, critters), `.fomap-bin-client` stops after the hash table
  and the static items. An empty client blob is the header plus two `uint32`
  zeros; a third one fails the load with "Not all data read".

### Reaching the world-reload path

Server tests default to the in-memory database, which means the branch a real
server takes on every restart — "Restore world" and `EntityManager::LoadEntities`
— never runs. Point the settings at the file-backed JSON storage instead
(`DbStorage = "JSON <dir>"`), let one server write the world and shut down, then
start a second server on the same directory. Two constraints:

- Runtime entities are temporary; call
  `EntityMngr.MakePersistent(entity, true, true)` on anything the restart is
  expected to find.
- Entities are reloaded through their owner. A critter created off-map is never
  reloaded, because critters are reached through the map or the global map they
  live on.

### Instantiating a 3D model headlessly

The Null renderer serves the whole model path, so a `ModelInstance` can be
created, posed and drawn without a GPU. The fixture chain is what makes it work:

- Bake a **triangle** mesh, not a single vertex — the info baker computes static
  bounds and rejects degenerate geometry.
- Produce the description with the real `ModelInfoBaker`, handing the
  `ModelSourceAsset` straight to its loader callback instead of reproducing a
  source-file format.
- The baking rig needs `Metadata.fometa-client` added as a **baked** file, and
  the mesh needs a **source** entry as well as its baked output, because the info
  baker resolves it through the source loader. Build the blob with
  `BakerTests::MakeMetadataBlob` / `MakeEmptyMetadataBlob`: registration rejects
  metadata without a version, which those helpers fill in.
- The runtime additionally requires `ModelAnimationInfo.foinfo` — a plain config
  keyed by the model resource name, with `BoundsVersion = 2`, the twelve
  model/view bounds keys, and at least one animation duration record.

Get the manager from the live client with
`client->SprMngr.GetSpriteFactory(typeid(ModelSpriteFactory)).dyn_cast<ModelSpriteFactory>()->GetModelMngr()`.

### Authoring static map content for a server fixture

A `.fomap-bin-server` blob is the format header, then the hash table, then the
critter records, then the item records. Each record is `ident` (`int64`), the prototype hash (`uint64`) and
a properties blob preceded by its `uint32` size. Writing a zero size fails with
"Unexpected end of buffer" — a default-constructed `Properties` still serializes
to a non-empty payload, so produce it with `props.StoreAllData(...)` rather than
assuming empty means zero bytes. With content present, map creation runs the
content generator instead of skipping it.

The client-side `.fomap-bin-client` blob is a different, shorter layout (header,
hash table and static items only).

### Writing into a real Maps root from the mapper

`SaveMap` / `SaveMapToDir` resolve the on-disk Maps root from an existing map
container, so a memory-only fixture cannot reach them. Point a resource pack at
a temp directory with `InputDirs = <dir>` (the plural key — the singular one is
silently ignored), drop a reference `.fomap` there, and set
`ProtoFileExtensions` to include `fomap` so the container is recognised. The
same fixture gives `DrawMapListWindowImGui` real entries to enumerate.

Prefer `SaveMapToDir` in tests: plain `SaveMap` falls back to the first source
file's directory when the map has no container of its own, which in a test
process is the working directory — it will write into the repository.

## Current test inventory

Current count: **103** `Test_*.cpp` suites.

### Essentials and low-level utilities

- `Source/Tests/Test_BaseLogging.cpp`
- `Source/Tests/Test_BasicCore.cpp`
- `Source/Tests/Test_CommonHelpers.cpp`
- `Source/Tests/Test_Compressor.cpp`
- `Source/Tests/Test_Containers.cpp`
- `Source/Tests/Test_DataSerialization.cpp`
- `Source/Tests/Test_DequeObject.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_ExceptionHandling.cpp`
- `Source/Tests/Test_ExtendedTypes.cpp`
- `Source/Tests/Test_FunctionObjects.cpp`
- `Source/Tests/Test_GenericUtils.cpp`
- `Source/Tests/Test_GlobalData.cpp`
- `Source/Tests/Test_HashedString.cpp`
- `Source/Tests/Test_Logging.cpp`
- `Source/Tests/Test_MemorySystem.cpp`
- `Source/Tests/Test_NetSockets.cpp`
- `Source/Tests/Test_Platform.cpp`
- `Source/Tests/Test_RandomGenerator.cpp`
- `Source/Tests/Test_SafeArithmetics.cpp`
- `Source/Tests/Test_SmartPointers.cpp`
- `Source/Tests/Test_StackTrace.cpp`
- `Source/Tests/Test_StringObject.cpp`
- `Source/Tests/Test_StringUtils.cpp`
- `Source/Tests/Test_StrongType.cpp`
- `Source/Tests/Test_Threading.cpp`
- `Source/Tests/Test_TimeRelated.cpp`
- `Source/Tests/Test_WorkThread.cpp`
- `Source/Tests/Test_WorkerPool.cpp`

### Configuration, data sources, files, and caches

- `Source/Tests/Test_CacheStorage.cpp`
- `Source/Tests/Test_ConfigFile.cpp`
- `Source/Tests/Test_DataSource.cpp`
- `Source/Tests/Test_FileSystem.cpp`
- `Source/Tests/Test_Settings.cpp`
- `Source/Tests/Test_SettingsStorage.cpp`

### Common runtime model

- `Source/Tests/Test_AnyData.cpp`
- `Source/Tests/Test_ApplicationHeadless.cpp`
- `Source/Tests/Test_Common.cpp`
- `Source/Tests/Test_EngineMetadata.cpp`
- `Source/Tests/Test_EntityLifecycle.cpp`
- `Source/Tests/Test_EntityProtos.cpp`
- `Source/Tests/Test_Geometry.cpp`
- `Source/Tests/Test_LineTracer.cpp`
- `Source/Tests/Test_MapLoader.cpp`
- `Source/Tests/Test_Movement.cpp`
- `Source/Tests/Test_PathFinding.cpp`
- `Source/Tests/Test_Properties.cpp`
- `Source/Tests/Test_ProtoManager.cpp`
- `Source/Tests/Test_TextPack.cpp`
- `Source/Tests/Test_Timer.cpp`
- `Source/Tests/Test_TwoDimensionalGrid.cpp`

### Networking and server/client integration

- `Source/Tests/Test_ClientDataValidation.cpp`
- `Source/Tests/Test_ClientEngine.cpp`
- `Source/Tests/Test_ClientRuntimeApi.cpp`
- `Source/Tests/Test_ClientServerIntegration.cpp`
- `Source/Tests/Test_DataBase.cpp`
- `Source/Tests/Test_EntitySync.cpp`
- `Source/Tests/Test_FogOfWar.cpp`
- `Source/Tests/Test_LocationAndEntityMgmt.cpp`
- `Source/Tests/Test_ModelAnimation.cpp`
- `Source/Tests/Test_NetBuffer.cpp`
- `Source/Tests/Test_NetworkClient.cpp`
- `Source/Tests/Test_NetworkServer.cpp`
- `Source/Tests/Test_NetworkUdp.cpp`
- `Source/Tests/Test_ServerAdvancedOps.cpp`
- `Source/Tests/Test_ServerEngine.cpp`
- `Source/Tests/Test_ServerEventContracts.cpp`
- `Source/Tests/Test_ServerItems.cpp`
- `Source/Tests/Test_ServerMapOperations.cpp`

### Scripting and script-visible APIs

- `Source/Tests/Test_AngelScriptAlignment.cpp`
- `Source/Tests/Test_AngelScriptAttributes.cpp`
- `Source/Tests/Test_AngelScriptBytecode.cpp`
- `Source/Tests/Test_AngelScriptCall.cpp`
- `Source/Tests/Test_CommonScriptMethods.cpp`
- `Source/Tests/Test_ScriptBuiltins.cpp`
- `Source/Tests/Test_ScriptEntityOps.cpp`
- `Source/Tests/Test_ServerScriptMethods.cpp`

### Bakers and tools

- `Source/Tests/Test_AngelScriptBaker.cpp`
- `Source/Tests/Test_BakerSetup.cpp`
- `Source/Tests/Test_ConfigBaker.cpp`
- `Source/Tests/Test_EffectBaker.cpp`
- `Source/Tests/Test_ImageBaker.cpp`
- `Source/Tests/Test_ImageWriter.cpp`
- `Source/Tests/Test_MapBaker.cpp`
- `Source/Tests/Test_Mapper.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_ModelBaker.cpp`
- `Source/Tests/Test_ModelBounds.cpp`
- `Source/Tests/Test_ModelMeshData.cpp`
- `Source/Tests/Test_ModelAnimationData.cpp`
- `Source/Tests/Test_ModelAnimationConverter.cpp`
- `Source/Tests/Test_ModelAnimationPoseProcedural.cpp`
- `Source/Tests/Test_ModelAnimationRuntime.cpp`
- `Source/Tests/Test_ModelSkeletonCompatibility.cpp`
- `Source/Tests/Test_ModelSpriteLayout.cpp`
- `Source/Tests/Test_ModelSourceLoader.cpp`
- `Source/Tests/Test_OzzAnimation.cpp`
- `Source/Tests/Test_ProtoBaker.cpp`
- `Source/Tests/Test_ProtoTextBaker.cpp`
- `Source/Tests/Test_RawCopyBaker.cpp`
- `Source/Tests/Test_TextBaker.cpp`
- `Source/Tests/Test_TextureAtlas.cpp`

The model-animation tests divide the production contract explicitly.
`Test_ModelMeshData.cpp` exercises the mandatory `LFMODMSH` schema-1 mesh-only
header and complete recursive payload codec. It covers geometry, skin palettes,
children, structural validation, trailing data, every truncated header size,
rejection of old headerless data, and exact byte compatibility with the original
schema-1 writer layout.
`Test_ClientEngine.cpp` also bakes a position-only OBJ through `ModelMeshBaker`
and preloads the resulting bytes through the real `ModelManager` parser. This
crosses the `BakerLib`/`ClientLib` boundary and catches payload-layout drift that
a second test-only parser could reproduce instead of detecting.
`Test_ModelSourceLoader.cpp` covers complete source validation, real minimal
OBJ/ASCII-FBX extraction, per-call cache single-flight behavior, shared results,
exception fan-out, and missing inputs. `Test_ModelAnimationData.cpp` exercises the
little-endian archive, joint-remap, and rig-manifest contracts, including
truncation, count/length bombs, ordering, metadata mismatches, and bindings.
`Test_ModelAnimationConverter.cpp` covers canonical conversion and the per-instance
runtime pose: unaligned/owned loading, body blending, movement replacement,
reverse and nearest sampling, stable storage, canonical resolution, and numeric
limits. `Test_ModelAnimationPoseProcedural.cpp` covers bounded procedural pre-rotations
and exact world-matrix overrides; `Test_ModelAnimationRuntime.cpp` covers the
validated direct-model rest path, canonical contributed-joint lookup, and
cross-model joint-link resolution without physical bones.
`Test_ModelBaker.cpp` covers source-backed model-info generation,
dependency-mtime invalidation, exact animation-geometry exceptions, `Base`,
reverse, case-insensitive lookup, and clip deduplication. `Test_ModelAnimation.cpp`
is the timeline/binding behavior gate: controller copies own mutable event state
while sharing only immutable Ozz clip metadata.

After source-loader, mesh-wire, or converter changes, `ForceBakeResources` is
the positive real-content gate: it must parse the project's actual selected FBX
sources and extract their animations successfully. Run ordinary
`BakeResources` afterward to check that the dependency-mtime contract leaves an
unchanged tree incremental-clean.

### Rendering/frontend smoke tests

- `Source/Tests/Test_ImGui.cpp` — pins the backend-less widget activation and
  window-state harness used by diagnostic-panel coverage.
- `Source/Tests/Test_EffekseerParticleRuntime.cpp` — runs cooked legacy and modern Effekseer
  effects through the native runtime's real Sprite/Ring callbacks and validates deterministic
  multi-instance topology, FOnline geometry, atlas UVs, all three Z-sort modes, Ring index-budget
  chunking, and facade-level scale reapplication without respawn or timing reset.
- `Source/Tests/Test_ParticleBaker.cpp` — covers `.efkproj` source discovery,
  `.spark`/`.efkproj` output-key mapping, generated binary validation, and
  rejection of authored `.spk`/`.efk` runtime inputs. The build/integration
  bake path exercises the native fixed-profile exporter on real XML projects.
- `Source/Tests/Test_Rendering.cpp`

## Validation routing by change type

- Essentials utilities: start with [Essentials.md](Essentials.md) and the essentials tests listed above.
- Config, file lookup, caches, resource packs: [ConfigurationAndDataSources.md](ConfigurationAndDataSources.md), parser/filesystem/cache tests, and affected bake/runtime consumers.
- BuildTools/CMake/generation: [BuildToolsPipeline.md](BuildToolsPipeline.md), [GeneratedApiAndMetadata.md](GeneratedApiAndMetadata.md), codegen/property/metadata tests, and at least one generated target.
- Bakers/resources: [BakingPipeline.md](BakingPipeline.md) and the matching baker tests.
- Runtime entity/map/persistence/networking: [EntityModel.md](EntityModel.md), [MapsMovementGeometry.md](MapsMovementGeometry.md), [Persistence.md](Persistence.md), [Networking.md](Networking.md), and the focused runtime tests.
- Client/frontend/server: [ClientRuntime.md](ClientRuntime.md), [FrontendAndRendering.md](FrontendAndRendering.md), [ServerRuntime.md](ServerRuntime.md), and the matching integration/smoke tests.
- Scripting: [Scripting.md](Scripting.md), [ScriptMethodsMap.md](ScriptMethodsMap.md), [Nullability.md](Nullability.md), and the script/baker/method tests.

## Adding or removing tests

1. Add the new `Source/Tests/Test_*.cpp` file with deterministic Catch2 tests.
2. Add it to `FO_TESTS_SOURCE` in `BuildTools/cmake/stages/EngineSources.cmake`.
3. Update this page and [../Source/Tests/README.md](../Source/Tests/README.md) so the inventory stays complete.
4. Run the focused test binary and, when practical, `RunUnitTests`.
5. If coverage behavior changed, verify the relevant coverage target.

## Validation checklist

1. Every current `Source/Tests/Test_*.cpp` file should appear in this page.
2. No deleted/nonexistent test file should be listed.
3. Target names should be described as generated from `FO_DEV_NAME`, not hard-coded as universal engine names.
4. If `TestingApp.cpp`, `FO_TESTS_SOURCE`, or coverage target wiring changes, update this page in the same change.
