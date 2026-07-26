# Documentation Verification Report

This report records source-grounded documentation verification passes for the engine docs in this checkout. It is not a replacement for the backlog; it records what was checked and which limitations remain.

## 2026-05-18 — source-tree and runtime model slice

Scope:

- `Docs/SourceTree.md`
- `Docs/EntityModel.md`
- `Docs/MapsMovementGeometry.md`
- `Docs/Persistence.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Applications/`, `Source/Client/`, `Source/Common/`, `Source/Server/`, `Source/Scripting/`, `Source/Tools/`, `Source/Frontend/`, `Source/Essentials/`, and `Source/Tests/` for the source-tree routing page.
- `Source/Common/Entity.*`, `EntityProperties.*`, `EntityProtos.*`, `Properties.*`, `PropertiesSerializator.*`, and `ProtoManager.*` for entity/property/prototype claims.
- `Source/Common/Geometry.*`, `LineTracer.*`, `Movement.*`, `PathFinding.*`, `MapLoader.*`, and `Source/Tools/MapBaker.*` for map/movement/geometry claims.
- `Source/Server/DataBase.*`, `Source/Server/DataBase-*.cpp`, and `Source/Tests/Test_DataBase.cpp` for persistence claims.

Results:

- Backticked source/build/doc path checks for this slice: no remaining missing paths after replacing the stale Docs/Testing.md future route in `SourceTree.md` and linking runtime map behavior to the now-present `ServerRuntime.md` / `ClientRuntime.md` pages.
- Symbol spot checks found the documented owners in current source: `EntityTypeDesc`, `FO_ENTITY_PROPERTY`, `ProtoEntity`, `PropertyRegistrator`, `GeometryHelper`, `FindPathInput`, `MovingContext`, `MapLoader`, `DataBaseImpl`, `RecoveryLogHandle`, and `CommitNextChange`.
- Current test inventory observed in this checkout: 79 `Source/Tests/Test_*.cpp` files.
- Promoted in `Docs/DocumentationBacklog.md`: `SourceTree.md`, `EntityModel.md`, `MapsMovementGeometry.md`, and `Persistence.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 client/frontend runtime slice below; continue next with `Docs/Networking.md` and `Docs/ServerRuntime.md`.
- Docs/Testing.md, Docs/Essentials.md, Docs/ConfigurationAndDataSources.md, and Docs/DocumentationMaintenance.md are still planned in this checkout unless created in a later pass.

## 2026-05-18 — client/frontend runtime slice

Scope:

- `Docs/ClientRuntime.md`
- `Docs/FrontendAndRendering.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Client/Client.*`, `Source/Client/ClientConnection.*`, `Source/Client/ResourceManager.*`, `Source/Client/MapView.*`, client entity/view classes, sprite factories, `Source/Client/RenderTarget.*`, `Source/Client/SpriteManager.*`, and `Source/Client/EffectManager.*` for client runtime composition, network dispatch, view-entity ownership, resources, sprites, effects, input mapping, and render-target claims.
- `Source/Frontend/Application*.cpp`, `Source/Frontend/Application.h`, `Source/Frontend/Rendering*.cpp`, `Source/Frontend/Rendering.h`, and `BuildTools/cmake/stages/Packages.cmake` for application services, headless/stub flows, renderer backends, package-boundary claims, and platform/frontend ownership.
- `Source/Tests/Test_ClientEngine.cpp`, `Source/Tests/Test_ClientServerIntegration.cpp`, `Source/Tests/Test_ClientDataValidation.cpp`, `Source/Tests/Test_ClientRuntimeApi.cpp`, and `Source/Tests/Test_Rendering.cpp` for the current engine-local validation surfaces named by the promoted pages.

Results:

- Backticked source/build/doc path checks for this slice: no missing paths after adding `Source/Tests/Test_Rendering.cpp` to the frontend/rendering page's inspected sources.
- Symbol spot checks found the documented owners and APIs in current source, including `ClientEngine`, `ClientConnection`, `GetClientResources`, `CreateNetworkConnection`, `TryFallbackToTcp`, client `Net_On...` handlers, `MapView`, client view classes, sprite/effect/render-target managers, `Application`, `AppWindow`, `AppInput`, `AppAudio`, `Renderer`, `RenderEffect`, `Null_Renderer`, `OpenGL_Renderer`, `Direct3D_Renderer`, and `IsRenderTargetFlipped`.
- Current focused test inventory observed for this slice: four `Source/Tests/Test_Client*.cpp` files plus `Source/Tests/Test_Rendering.cpp`.
- Promoted in `Docs/DocumentationBacklog.md`: `ClientRuntime.md` and `FrontendAndRendering.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 networking/server runtime slice below; continue next with `Docs/ClientUpdater.md` and updater boundary checks.

## 2026-05-18 — networking/server runtime slice

Scope:

- `Docs/Networking.md`
- `Docs/ServerRuntime.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Common/NetBuffer.*` and `Source/Common/NetworkUdp.*` for message framing, hash/debug-hash serialization, and ordered UDP behavior.
- `Source/Client/NetworkClient*` and `Source/Server/NetworkServer*` for transport-neutral client/server abstractions plus interthread, socket, UDP, ASIO, and WebSocket implementations.
- `Source/Server/Server.*`, `Source/Server/EntityManager.*`, `MapManager.*`, `CritterManager.*`, `ItemManager.*`, `Player.*`, `Critter.*`, `Map.*`, `Location.*`, `Item.*`, `ClientDataValidation.*`, and `UpdaterBackend.*` for authoritative server runtime ownership, entity/session flow, validation, managers, movement, persistence handoff, and updater hosting.
- `Source/Tests/Test_NetworkUdp.cpp`, `Source/Tests/Test_NetworkClient.cpp`, `Source/Tests/Test_NetworkServer.cpp`, `Source/Tests/Test_ServerEngine.cpp`, `Source/Tests/Test_ServerItems.cpp`, `Source/Tests/Test_ServerMapOperations.cpp`, `Source/Tests/Test_ServerAdvancedOps.cpp`, `Source/Tests/Test_ServerScriptMethods.cpp`, `Source/Tests/Test_ClientServerIntegration.cpp`, `Source/Tests/Test_ClientDataValidation.cpp`, and `Source/Tests/Test_DataBase.cpp` for the current validation surfaces named by the promoted pages.

Results:

- Added a `Source paths inspected` section to `Docs/Networking.md` and replaced its generic integration-test wording with the concrete current `Source/Tests/Test_ClientServerIntegration.cpp` reference.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `NetBuffer`, `NetOutBuffer`, `NetInBuffer`, `NetworkClientConnection`, `NetworkServerConnection`, `NetworkServer`, `UdpOrderedChannel`, UDP packet helpers, `ServerEngine`, server init/job methods, server events, `EntityManager`, `MapManager`, `CritterManager`, `ItemManager`, `Player`, inbound `Process_*` handlers, client-data validation functions, movement helpers, and `UpdaterBackend`.
- Current focused test inventory observed for this slice: three `Source/Tests/Test_Network*.cpp` files, five `Source/Tests/Test_Server*.cpp` files, plus `Source/Tests/Test_ClientServerIntegration.cpp`, `Source/Tests/Test_ClientDataValidation.cpp`, and `Source/Tests/Test_DataBase.cpp`.
- Promoted in `Docs/DocumentationBacklog.md`: `Networking.md` and `ServerRuntime.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 client updater slice below; continue next with platform debugging docs: `Docs/WebDebugging.md`, `Docs/AndroidDebugging.md`, and `Docs/Debugging.md`.

## 2026-05-18 — client updater/runtime split slice

Scope:

- `Docs/ClientUpdater.md`
- updater-boundary references in `Docs/ServerRuntime.md` and `Docs/ClientRuntime.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Applications/ClientApp.cpp` and `Source/Applications/ClientLib.cpp` for host/runtime loading, fallback, staged binary promotion, reload results, and platform gating.
- `Source/Client/ClientRuntimeApi.*`, `Source/Client/Updater.*`, `Source/Frontend/ApplicationInit.cpp`, `Source/Essentials/DiskFileSystem.*`, `Source/Essentials/Platform.*`, `Source/Common/Common.h`, and `Source/Common/Settings.inc` for runtime ABI, updater protocol versioning, update targets, installed-client writable paths, disk hashing/cache behavior, and updater settings.
- `Source/Server/UpdaterBackend.*` and `Source/Server/Server.cpp` for server-side descriptor generation, file serving, target-specific binaries, and `UpdateFileMaxPortionSize` use.
- `BuildTools/cmake/stages/Applications.cmake`, `BuildTools/package.py`, and `BuildTools/msicreator/createmsi.py` for client host/library build gates, runtime binary packaging/staging, and Windows MSI installer metadata.
- `Source/Tests/Test_ClientRuntimeApi.cpp` for runtime ABI coverage. Embedding-project updater pipeline tests are project-owned supplemental checks and are not engine documentation dependencies.

Results:

- Added a `Source paths inspected` section to `Docs/ClientUpdater.md`.
- Replaced obsolete package-entry wording with the current `build_runtime_update_target_name` owner in `BuildTools/package.py`.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `UpdaterBackend`, `LoadFromClientResources`, `ProcessUpdateFile`, `GetUpdateDescriptor`, `FO_CLIENT_RUNTIME_HOST_ABI_VERSION`, `ClientRuntimeMetadata`, `ClientRuntimeExports`, `ClientRuntimeResult`, `FO_QueryClientRuntimeExports`, `ApplyStagedBinaryUpdate`, `GetClientRuntimeLivePath`, `MakeClientRuntimeStagingPath`, `RunClientFromLibrary`, `RunEmbeddedOrLoadedClient`, `ResolveRequestedClientRuntime`, `ResolveUserWritablePath`, `fs_make_writable_path`, `Platform::GetUserDataBase`, `CanSelfUpdateNativeModules`, `FO_UPDATER_VERSION`, `UpdateFileTarget`, `ClientBinaries`, `ClientResources`, `GetCurrentBinaryUpdateTargetName`, `UpdateFileMaxPortionSize`, `UpdateFilesInMemory`, `PlatformBinaries`, and `build_runtime_update_target_name`.
- Promoted in `Docs/DocumentationBacklog.md`: `ClientUpdater.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 platform debugging slice below; continue next with `Docs/Architecture.md` and `Docs/Applications.md`.

## 2026-05-18 — platform debugging slice

Scope:

- `Docs/WebDebugging.md`
- `Docs/AndroidDebugging.md`
- `Docs/Debugging.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `BuildTools/buildtools.py`, `BuildTools/prepare-workspace.sh`, `BuildTools/prepare-win-workspace.ps1`, `ThirdParty/emscripten`, parent `.vscode/tasks.json`, parent `.vscode/launch.json`, parent `CMakePresets.json`, `LastFrontier.fomain`, `Scripts/Scenes.fos`, and `Scripts/GameState.fos` for web build/package/launch and remote-scene debugging flow.
- `BuildTools/android_device.py`, `BuildTools/package.py`, `BuildTools/android-project/`, `FOnlineActivity.java`, Android SDK/NDK pins, parent VS Code tasks, package definitions, CI workflow, and project config for Android package/install/launch/resource-copy behavior.
- `BuildTools/natvis/`, `BuildTools/cmake/stages/Finalize.cmake`, `BuildTools/cmake/helpers/Build.cmake`, `Source/Essentials/StackTrace.*`, `BaseLogging.*`, `ExceptionHandling.*`, `Source/Scripting/AngelScript/AngelScriptContext.cpp`, `Source/Frontend/ApplicationInit.cpp`, `Source/Tests/Test_StackTrace.cpp`, and `Source/Tests/Test_ExceptionHandling.cpp` for native visualizers, stack traces, exception callbacks, logging/crash paths, and debugger routing.

Results:

- Added `Source paths inspected` sections to the web, Android, and native debugging docs.
- Corrected stale stack/exception documentation from the previous context-object / deferred-log callback model to the current `CatchedStackTraceData`, `FormatStackTrace(const CatchedStackTraceData&)`, `WriteLogMessage`, and `SafeWriteStackTrace` model.
- Corrected the AngelScript provider name from the old stack-frame wording to current `CollectScriptStackLayers` / script-layer behavior.
- Corrected web preset references to the parent `../../CMakePresets.json` path.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source/config, including `package-web-debug`, `prepare-host-workspace`, `Workspace/web-debug`, `package-android-debug`, `android-arm64`, `launch-game`, `FOnlineActivity`, `ClientNetwork.ServerHost`, `FO_STACK_TRACE_ENTRY`, `StackTraceData`, `CatchedStackTraceData`, `SetScriptStackTraceProvider`, `CollectScriptStackLayers`, `ResolveStackTrace`, `FormatStackTrace`, `SafeWriteStackTrace`, `GetStackTraceEntry`, `BaseEngineException`, `WriteLogMessage`, and `SetAsyncLogWriting`.
- Promoted in `Docs/DocumentationBacklog.md`: `WebDebugging.md`, `AndroidDebugging.md`, and `Debugging.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 architecture/applications slice below; continue next with the build/generation slice.

## 2026-05-18 — architecture/applications slice

Scope:

- `Docs/Architecture.md`
- `Docs/Applications.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Applications/` for all current app entry-point files.
- `Source/Common/EngineBase.*`, `Source/Common/Entity.*`, `Source/Common/ScriptSystem.*`, `Source/Client/Client.h`, `Source/Server/Server.h`, `Source/Frontend/Application.h`, and `Source/Frontend/ApplicationInit.cpp` for the architecture layer map.
- `BuildTools/cmake/stages/Applications.cmake` and `BuildTools/cmake/helpers/Build.cmake` for application target wiring and helper ownership.

Results:

- Added `Source paths inspected` sections to the architecture and applications docs.
- Replaced stale future-doc wording with links to now-present `BuildToolsPipeline.md`, `Scripting.md`, `ServerRuntime.md`, `BakingPipeline.md`, and `GeneratedApiAndMetadata.md`; kept testing routed to `Source/Tests/README.md` until a dedicated Docs/Testing.md exists.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol/path spot checks found all documented application entry points and app-wiring helpers in current source.
- Promoted in `Docs/DocumentationBacklog.md`: `Architecture.md` and `Applications.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 build/generation slice below; continue next with the script-boundary slice.

## 2026-05-18 — build/generation slice

Scope:

- `Docs/BuildToolsPipeline.md`
- `Docs/BakingPipeline.md`
- `Docs/GeneratedApiAndMetadata.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `BuildTools/Init.cmake`, all current `BuildTools/cmake/stages/*.cmake`, `BuildTools/cmake/helpers/*.cmake`, `BuildTools/codegen.py`, and `BuildTools/package.py` for staged build/generation/package ownership.
- `Source/Applications/BakerApp.cpp`, `Source/Applications/BakerLib.cpp`, `Source/Tools/Baker.*`, all current `Source/Tools/*Baker.*` implementations, `BuildTools/cmake/stages/ScriptsAndBaking.cmake`, and baker tests for baking behavior.
- `BuildTools/cmake/stages/Codegen.cmake`, `BuildTools/cmake/stages/EngineSources.cmake`, metadata helpers/state, `BuildTools/codegen.py`, `Source/Common/MetadataRegistration.*`, `Source/Common/MetadataRegistration.template.cpp`, `Source/Common/GenericCode.template.cpp`, `Source/Common/Properties.*`, `Source/Common/Entity.*`, `Source/Tools/MetadataBaker.*`, metadata/property tests, and `PUBLIC_API.md` for generated API and metadata behavior.

Results:

- Added `Source paths inspected` sections to the BuildTools, baking, and generated API/metadata docs.
- Replaced remaining future-script-doc wording in build routing with a real link to the present `Scripting.md` page.
- Confirmed current built-in baker owners include `ModelMeshBaker` / `ModelInfoBaker` under `Source/Tools/` and current model-baker coverage in `Source/Tests/Test_ModelBaker.cpp`.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `AddStageHook`, `AddExecutableApplication`, `AddSharedApplication`, `BakeResources`, `ForceBakeResources`, `CompileAngelScript`, `CompileMonoScripts`, `BaseBaker`, `SetupBakers`, `MasterBaker`, `MetadataBaker`, `CodeGeneration`, `ForceCodeGeneration`, `FO_SOURCE_META_FILES`, `FO_MONO_SOURCE`, `FO_ADDED_COMMON_HEADERS`, `FO_EMBEDDED_DATA_CAPACITY`, `FO_INTERNAL_CONFIG_CAPACITY`, `RegisterDynamicMetadata`, `MetadataRegistration` templates, `GenericCode-Template`, and `PropertyRegistrator`.
- Promoted in `Docs/DocumentationBacklog.md`: `BuildToolsPipeline.md`, `BakingPipeline.md`, and `GeneratedApiAndMetadata.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 scripting/nullability slice below; continue next with the tools/mapper slice.

## 2026-05-18 — scripting/nullability slice

Scope:

- `Docs/Scripting.md`
- `Docs/ScriptMethodsMap.md`
- `Docs/Nullability.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Common/ScriptSystem.*`, `Source/Scripting/AngelScript/AngelScriptScripting.*`, `AngelScriptBackend.*`, `AngelScriptAttributes.cpp`, `AngelScriptCall.cpp`, `AngelScriptEntity.cpp`, `AngelScriptGlobals.cpp`, `AngelScriptRemoteCalls.cpp`, `AngelScriptReflection.cpp`, engine core scripts, Mono/Native roots, and `BuildTools/cmake/stages/ScriptsAndBaking.cmake` for scripting runtime/build flow.
- All 18 current `Source/Scripting/*ScriptMethods.cpp` files for native exported method ownership and current export counts.
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`, `Source/Tools/MetadataBaker.cpp`, `BuildTools/codegen.py`, `Source/Common/ScriptSystem.h`, `Source/Essentials/BasicCore.h`, nullable analyzer tools under `../Tools/NullableEstimate/`, parent VS Code/CI task wiring, and nullable/script tests for nullability contracts.

Results:

- Confirmed the current native script method map: 874 `///@ ExportMethod` declarations across 18 script method files after the 2026-05-21 default-argument overload collapse.
- Corrected stale nullability workflow wording: current nullable appliers preserve author-chosen markers and remove redundant guards; they do not own automatic contract inference.
- Replaced stale parent docs routes in `Nullability.md` with current engine docs and the current `Source/Tests/README.md` testing source of truth.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `ScriptSystem`, `ScriptSystemBackend`, `RegisterBackend`, `MapScriptTypes`, `InitModules`, `FindFunc`, `CheckFunc`, `CallFunc`, `CallAdminFunc`, `NativeDataProvider`, `CheckArgNotNull`, `CheckReturnNotNull`, `InitAngelScriptScripting`, `CompileAngelScript`, `AngelScriptBackend`, `RegisterMetadata`, `CompileTextScripts`, `LoadBinaryScripts`, `StripNullableTypeSuffix`, and `is_validated_pointer_meta_type`.
- Promoted in `Docs/DocumentationBacklog.md`: `Scripting.md`, `ScriptMethodsMap.md`, and `Nullability.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 tools/mapper slice below; continue next with the planned docs set.

## 2026-05-18 — tools/mapper slice

Scope:

- `Docs/Tools.md`
- `Docs/MapperTools.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- All current `Source/Tools/*.h` and `Source/Tools/*.cpp` files, tool application entry points under `Source/Applications/`, and focused baker tests for reusable tool ownership.
- `Source/Applications/MapperApp.cpp`, `Source/Tools/Mapper.*`, `Source/Scripting/MapperGlobalScriptMethods.cpp`, `Source/Scripting/CommonGlobalScriptMethods.cpp`, `Source/Client/MapView.*`, and `Source/Common/Geometry.cpp` for mapper lifecycle, mapper automation helpers, screenshot/readback flow, and map/camera transform claims.
- Embedding-project examples explicitly marked as examples: `../../Scripts/MapperRender.fos`, `../../Tools/MapPreview/generate_map_preview.py`, `../../Tools/MapPreview/map_preview_overrides.ini`, and `../../LastFrontier.fomain`.

Results:

- Added `Source paths inspected` to `Docs/MapperTools.md` and kept project-specific map-preview/checkpoint details explicitly routed through `../../...` embedding-project paths.
- Replaced stale parent-doc links in `MapperTools.md` with engine-local `Architecture.md` / `Scripting.md` links where the owner now exists in engine docs; project-only build/checkpoint routes remain plain embedding-project paths.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `MasterBaker`, `BaseBaker`, `SetupBakers`, `MapperEngine`, `MapperMainLoop`, `DrawMapperFrame`, `ProcessMapperInputEvent`, `LoadMapFromText`, `LoadMap`, `ShowMap`, `SaveCurrentMap`, `SaveMap`, `Mapper_Game_*` exports, `Common_Game_RequestQuit`, `MapView::SetScreenSize`, `MapView::InstantScrollTo`, `MapView::InstantZoom`, `WriteSimpleTga`, and `GetHexOffset`.
- Promoted in `Docs/DocumentationBacklog.md`: `Tools.md` and `MapperTools.md` from `drafted` to `verified`.

Follow-up:

- Completed by the later 2026-05-18 planned-docs completion slice below; the initial documentation backlog is now complete.
## 2026-05-18 — planned-docs completion slice

Scope:

- `Docs/Essentials.md`
- `Docs/ConfigurationAndDataSources.md`
- `Docs/Testing.md`
- `Docs/DocumentationMaintenance.md`
- `Docs/README.md`
- `Source/Tests/README.md`
- `AGENTS.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `Source/Essentials/*.h`, `Source/Essentials/*.cpp`, `BuildTools/cmake/stages/EngineSources.cmake`, and essentials tests for the low-level foundation page.
- `Source/Common/ConfigFile.*`, `Settings.*`, `Settings.inc`, `DataSource.*`, `FileSystem.*`, `CacheStorage.*`, `Source/Essentials/DiskFileSystem.*`, `Source/Client/ResourceManager.*`, baker/config consumers, BuildTools generation/baking/package stages, and focused config/data-source/cache/filesystem tests for configuration/data-source routing.
- `Source/Applications/TestingApp.cpp`, all 79 current `Source/Tests/Test_*.cpp` files, `FO_TESTS_SOURCE` in `BuildTools/cmake/stages/EngineSources.cmake`, generated test/coverage target wiring in `BuildTools/cmake/stages/Applications.cmake`, coverage setup in `BuildTools/cmake/stages/Init.cmake`, `BuildTools/codecoverage.py`, and validator wrappers for the test-suite page.
- `../AGENTS.md`, `README.md`, `Docs/README.md`, `Docs/DocumentationBacklog.md`, `Docs/DocumentationExpansionPlan.md`, `Docs/DocumentationResearchTemplate.md`, and this report for documentation-maintenance workflow.

Results:

- Created the final four planned docs and linked them from `Docs/README.md` and `AGENTS.md` where appropriate.
- Updated `Source/Tests/README.md` from a partial stale inventory to a complete short source-tree entry point linked to `Docs/Testing.md`.
- Confirmed the current test inventory is 79 `Source/Tests/Test_*.cpp` suites and listed every suite in `Docs/Testing.md` and `Source/Tests/README.md`.
- Promoted in `Docs/DocumentationBacklog.md`: `Essentials.md`, `ConfigurationAndDataSources.md`, `Testing.md`, and `DocumentationMaintenance.md` from `planned` to `verified`.
- Marked the initial documentation backlog plan complete. Future doc work should be driven by new source changes, stale findings, or explicit requests.

Follow-up:

- No backlog-planned docs remain. Re-run the documented checks whenever source or docs change.
## 2026-05-18 — build workflow completion slice

Scope:

- `Docs/BuildWorkflow.md`
- `README.md`
- `Docs/DocumentationExpansionPlan.md`
- `Docs/DocumentationBacklog.md`

Source areas checked:

- `../CMakeLists.txt`, `../BuildTools/README.md`, `../BuildTools/Init.cmake`, `../BuildTools/validate.sh`, `../BuildTools/validate.cmd`, `../BuildTools/buildtools.py`, staged CMake files under `../BuildTools/cmake/stages/`, helpers under `../BuildTools/cmake/helpers/`, `../Source/Applications/TestingApp.cpp`, and `../Source/Tests/README.md`.

Results:

- Added source-inspection provenance and validation checklist to `BuildWorkflow.md`.
- Routed build validation to the newly created `Testing.md`, `Essentials.md`, and `ConfigurationAndDataSources.md` where appropriate.
- Added the final planned docs to the root `README.md` documentation index and refreshed `DocumentationExpansionPlan.md` current-baseline list.
- Promoted `BuildWorkflow.md` from `drafted` to `verified`; no non-legend `planned`, `researching`, or `drafted` backlog entries remain.

Follow-up:

- The initial documentation backlog remains complete; future work should be driven by source changes, stale findings, or new requested topics.

## 2026-07-08 — native conventions and safety contracts

Scope:

- `Docs/SmartPointers.md`
- `Docs/ExceptionSafety.md`
- `Docs/ThreadSafetyAnalysis.md`
- `Docs/DocumentationBacklog.md`
- `Docs/DocumentationExpansionPlan.md`

Source areas checked:

- `Source/Essentials/SmartPointers.h`, `Source/Essentials/BasicCore.h`, and `Source/Essentials/ExceptionHandling.cpp` for the native pointer vocabulary, non-null enforcement, and the `FO_BASIC_STRONG_ASSERT` bridge.
- `Source/Essentials/MemorySystem.h`, `Source/Essentials/Containers.h`, `Source/Essentials/CommonHelpers.h`, `Source/Server/DataBase.cpp`, `Source/Server/WorkerPool.cpp`, `Source/Tests/Test_EntityLifecycle.cpp`, and `Source/Tests/Test_ServerMapOperations.cpp` for exception-safety and lifecycle-invariant claims.
- `Source/Essentials/Threading.h` and `BuildTools/cmake/stages/Init.cmake` for `FO_TSA_*` annotations, `fo::` lock wrappers, and `-Wthread-safety` / `-Werror=thread-safety` Clang enforcement.
- Embedding-project example path `../../Tools/SmartPointerAudit/smart_pointer_audit.py` for the non-normative smart-pointer audit reference.

Results:

- Recorded the post-initial-backlog native-conventions docs in `Docs/DocumentationBacklog.md` with current source-validation status.
- Marked `Docs/DocumentationExpansionPlan.md` as a completed historical roadmap so future doc work starts from source changes, stale findings, or explicit requests instead of a duplicate inventory.
- Confirmed the native-conventions docs are routed from `AGENTS.md`, root `README.md`, and `Docs/README.md`.
- Markdown link check over `README.md`, `AGENTS.md`, `Docs/**/*.md`, `Source/README.md`, `Source/Tests/README.md`, and `BuildTools/README.md`: passed.
- Backticked source/build/doc path spot checks for this slice: no missing checked paths.
- `git diff --check`: passed.

Follow-up:

- No queued documentation slice remains in the expansion plan. Future native-convention updates should be driven by source changes and recorded here plus in `Docs/DocumentationBacklog.md`.

## 2026-07-10 - production documentation program audit and plan

Scope:

- `Docs/ProductionDocumentationPlan.md`
- `Docs/README.md`
- `Docs/DocumentationBacklog.md`
- `Docs/DocumentationExpansionPlan.md`
- `Docs/DocumentationVerificationReport.md`

Repository areas checked:

- Engine `README.md`, `AGENTS.md`, `TUTORIAL.md`, `PUBLIC_API.md`, all current `Docs/*.md`, source/build README files, `_config.yml`, `.github/workflows/validate.yml`, and the completed documentation expansion artifacts.
- Current source/build coverage under `Source/`, `BuildTools/`, `Resources/`, metadata annotations, `Source/Common/Settings.inc`, script method files, `Source/Tests/Test_*.cpp`, and `BuildTools/validation-project`.
- The Last Frontier documentation corpus at project commit `805caa79976b7cf4f81e46e1cf9ca0f1ea96ba43` as the primary mature embedding-project sample.
- Public `cvet/fonline-tla` files and recent history at commit `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` as the second integration sample.
- Diataxis, official GitHub Pages/Jekyll and custom-domain guidance, and the public `llms.txt` proposal for information-architecture, publication, localization, and AI-delivery choices.

Audit results:

- Confirmed the existing engine set is a strong internal reference but lacks a working beginner tutorial, current public API contract, generated full reference, canonical starter project, docs CI/site, translation system, and AI-readable manifest/evaluation.
- Counted 33 current Markdown links that escape a standalone engine root and 17 engine docs containing Last Frontier names or parent-project path markers; the new production plan itself has no project-path dependency.
- Confirmed current manual-inventory drift: 947 `///@ ExportMethod` declarations versus 932/874 documented counts, and 84 `Test_*.cpp` files versus 81/79 documented counts.
- Confirmed `Source/Common/Settings.inc` contains 265 fixed/variable settings with no complete generated settings reference.
- Confirmed `TUTORIAL.md`, `PUBLIC_API.md`, and the BuildTools README still contain placeholders or stale public-entry text.
- Confirmed TLA contains engine-owned animation documentation worth migrating, while its advertised scripting API URL returned HTTP 404 during this audit.

Plan results:

- Added a ten-phase execution roadmap covering standalone independence, docs platform and CI, public API generation, starter/tutorial work, content/tooling coverage, operations/migrations, public showcase repositories, AI delivery, and final Russian localization.
- Defined four public example repositories: a canonical template, a minimal multiplayer tutorial, a content/rendering showcase, and a native-extension sample.
- Kept the existing Markdown-to-GitHub Pages/Jekyll publication contract and `fonline.ru` custom domain, with mirrored `Docs/en/` and `Docs/ru/` trees instead of `Docs.EN` / `Docs.RU` sibling roots or a separate site framework.
- Added measurable launch gates, page definition of done, ownership/stability rules, cross-project best-practice promotion criteria, risks, and the first execution slice.
- Routed the active plan from `Docs/README.md`, `DocumentationBacklog.md`, and the completed historical expansion plan.

Mechanical checks:

- Local Markdown link check over 46 engine documentation entry files: passed in the current embedding checkout.
- Production-plan standalone local-link and repository-boundary check: passed.
- Production-plan heading/fence and whitespace checks: passed.
- Existing GitHub Pages contract check: root `CNAME` is `fonline.ru` and `_config.yml` selects `jekyll-theme-slate`.
- `git diff --check` for tracked edits plus an explicit untracked-plan whitespace check: passed; line-ending conversion warnings remain informational.
- Staged area: empty.

Follow-up:

- Start with the plan's `First execution slice`: ownership manifest, standalone docs CI, removal of root-escaping dependencies, generated inventories, public-contract/GitHub Pages ADRs, and the first runnable starter path.

## 2026-07-10 - standalone docs infrastructure and first starter lesson

Scope:

- `Docs/documentation-manifest.json`, `BuildTools/docs_validate.py`, `BuildTools/docs_inventory.py`, their focused tests, and `.github/workflows/validate.yml`.
- Standalone rewrites for the project-dependent platform/debugging/updater/mapper/nullability/testing pages and removal of all root-escaping local links.
- `Docs/Decisions/0001-github-pages-markdown-publication.md` and `Docs/Decisions/0002-public-api-stability-contract.md`.
- `Examples/MinimalProject/`, `BuildTools/buildtools.py`, `TUTORIAL.md`, and the first-run routes in `GettingStarted.md`, `EmbeddingProject.md`, and `BuildWorkflow.md`.

Source areas checked:

- All maintained Markdown entry points, root `CNAME`, `_config.yml`, current engine-owned source references, and the documentation publication/ownership plan.
- All 18 `Source/Scripting/*ScriptMethods.cpp` files, all 84 `Source/Tests/Test_*.cpp` files, and `Source/Common/Settings.inc` for deterministic inventory generation.
- BuildTools validation-project preparation, CMake stages, baker/resource-pack behavior, AngelScript side preprocessing, application settings loading, headless server startup, and database/network initialization.

Results:

- Added a machine-readable ownership/classification manifest covering 49 maintained Markdown entries, including the canonical example README.
- Added a fast standalone validator for manifest coverage, source paths, local links/anchors, repository-boundary escapes, placeholder honesty, Pages domain/config, and generated-inventory freshness.
- Added deterministic source inventory output for 947 exported script methods, 84 native test files, and 265 fixed/variable settings; removed stale hand-maintained method/test counts from prose.
- Replaced the tutorial placeholder with a tested headless lesson and replaced the former empty internal validation scaffold with `Examples/MinimalProject`.
- Added Windows and Linux starter targets to CI. Windows x64 passed locally; Linux remains pending because WSL is unavailable on this host and must be confirmed by GitHub Actions.
- Hardened the smoke with a 60-second timeout, process-exit validation, and required AngelScript lifecycle markers. BuildTools now recreates the disposable project copy so removed files cannot survive between runs.

Integration findings incorporated into the example and tutorial:

- The `Config` baker requires a complete explicit engine settings set, so the first unpackaged milestone intentionally omits that packaging layer.
- Importing all `CoreScripts` also imports project contracts such as world-time and generated GUI symbols; the minimal server pack contains only its owned script.
- AngelScript baking still validates each runtime side, so the module retains one common declaration outside its server-only lifecycle block.
- Unpackaged runtime startup consumes explicit `.fomain` values; the starter therefore declares memory storage, ID allocation, network ports, and a smoke-only networking override.

Mechanical checks:

- `python BuildTools/tests/test_docs_inventory.py`: 2 tests passed.
- `python BuildTools/tests/test_docs_validate.py`: 8 tests passed.
- `python BuildTools/docs_inventory.py --check`: current at 947 methods, 84 tests, and 265 settings.
- `python BuildTools/docs_validate.py`: passed for all 49 maintained Markdown entries.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: passed after a clean project copy, resource bake, both lifecycle markers, and clean server shutdown.
- Python byte-compilation for the new validation/generation/smoke scripts: passed.
- Engine-style `clang-format` dry-run for the new C++ and AngelScript files: passed with the locally available formatter; the repository-required version 20 is not installed on this host.
- `git diff --check`: passed. Engine and parent-project staging areas are empty.

Follow-up:

- Confirm `linux-starter-smoke` in GitHub Actions, then mark the first execution slice complete.
- Continue with the generated public API model and GitHub Pages-compatible Jekyll preview rather than broad translation or prose migration.

## 2026-07-10 - GitHub Pages-compatible preview and publication contract

Scope:

- `_config.yml`, `.ruby-version`, `Gemfile`, `.gitignore`, and `.github/workflows/validate.yml`.
- `Docs/SitePublication.md`, `Docs/documentation-manifest.json`, and the human/AI entry-point routes.
- `BuildTools/docs_validate.py` and `BuildTools/tests/test_docs_validate.py`.
- Production-plan and backlog status for the documentation platform slice.

Evidence checked:

- Current GitHub Pages dependency set: Ruby `3.3.4`, `github-pages` `232`, Jekyll `3.10.0`, and Slate `0.2.0`.
- Official `actions/jekyll-build-pages@v1` source, inputs, and GitHub Pages-compatible Docker build behavior.
- Live `https://fonline.ru` landing page, root `CNAME`, and public DNS resolution to `185.199.108.153` through `185.199.111.153`.
- Public repository data and an unauthenticated Pages API request. The exact configured production source branch/folder could not be proved without repository administrator access and remains explicitly pending.

Results:

- Pinned the local compatibility environment in `.ruby-version` and `Gemfile`; `Gemfile.lock` is intentionally ignored as recommended for this Pages workflow.
- Extended `_config.yml` with the production URL/repository, strict front matter, supported relative-link rendering, and local/build-tree exclusions while retaining the Slate theme and Markdown source model.
- Added a dependent `Build documentation site` CI job that renders with the official Pages action and uploads `documentation-site-<commit-sha>` from `_site/` for 14 days. It validates only and does not replace the production Pages route.
- Added the engine-owned publication/operator guide and routed it from `README.md`, `Docs/README.md`, `AGENTS.md`, and the documentation maintenance guide.
- Expanded the machine-readable publication contract and validator so domain, config, Ruby version, Pages gem pin, source-verification state, Jekyll action, destination, and artifact upload cannot drift silently.
- Maintained an honest `pending-admin-verification` state for Pages source branch/folder instead of inferring an administrator setting from public output.

Mechanical checks:

- `python BuildTools/tests/test_docs_validate.py`: 11 tests passed, including wrong gem pin, missing Jekyll action, and incomplete verified-source failures.
- `python BuildTools/tests/test_docs_inventory.py`: 2 tests passed.
- `python BuildTools/docs_inventory.py --check`: current at 947 methods, 84 tests, and 265 settings.
- `python BuildTools/docs_validate.py`: passed for all 50 maintained Markdown entries.
- Python byte-compilation of the edited validator and tests: passed.
- Isolated Ruby `3.3.4` started successfully from `Workspace/`; local bundle installation reached native extensions and then correctly required RubyInstaller Devkit/MSYS2, which is unavailable on this host. No system Ruby was installed.

Follow-up:

- Observe the first green `Build documentation site` job and inspect its `_site` artifact.
- Have a repository administrator record the actual Pages source mode, branch, and folder in the manifest and this guide.
- Keep the existing production route unchanged; continue with navigation/search only after the current render is confirmed.

## 2026-07-10 - canonical native-codegen API model

Scope:

- `BuildTools/codegen.py`, `BuildTools/docs_api.py`, and `BuildTools/tests/test_docs_api.py`.
- `Docs/generated/api.json`, `Docs/documentation-manifest.json`, `BuildTools/docs_validate.py`, and documentation CI.
- `PUBLIC_API.md`, `Docs/GeneratedApiAndMetadata.md`, `Docs/ScriptMethodsMap.md`, and documentation maintenance routes.
- Production-plan and backlog status for Phase 3.

Source areas checked:

- Typed codegen tag records and parsers for exported enums, value/reference types, entities, properties, methods, events, settings, hooks, and migration rules.
- Metadata registration target expansion, common/client/mapper side rules, method receiver expansion, nullability/default normalization, and value-type layout parsing.
- All non-test C++/header/include inputs under `Source/`, with the independent source inventory retained as a cross-check.
- ADR 0002 stability defaults and the production plan's required canonical-model fields and explicit domain boundaries.

Results:

- Added parallel source provenance for parsed codegen tags and a resettable metadata parse session. Provenance is not added to tag dataclasses or compatibility-hash inputs, so documentation paths/lines do not alter the runtime compatibility contract.
- Added a deterministic `engine-native-codegen` JSON model with flat addressable symbols, normalized signatures, runtime sides, receivers, arguments/defaults/nullability, property/setting mutability, exact default command-line redaction state, descriptions, source locations, and explicit stability/version/example fields.
- Single symbols use their family ID; overloads retain the family ID and receive a deterministic signature-hash suffix. Generation rejects duplicate final IDs.
- The generated snapshot contains 2,459 addressable symbols: 947 native methods, 133 properties, 120 events, 265 settings, plus entities, enums/values, value/reference types and members, and migration rules.
- Every unclassified symbol is `internal`. The model lists project script metadata/remote calls, CMake/CLI/package contracts, and native-extension ABI details as excluded rather than implying false completeness.
- Added generated description/stability/provenance coverage metrics. The snapshot exposes a large source-comment backlog instead of filling missing descriptions with guessed prose.
- Added API-model freshness to the manifest, standalone validator, and fast CI job; generated JSON remains checked in for GitHub Pages, offline use, and AI retrieval.
- Kept `PUBLIC_API.md` as a visible placeholder route until generated human pages, explicit stability, remaining domains, and API-diff enforcement are complete.

Mechanical checks:

- Reused the codegen metadata parser twice in one Python process: both passes produced the expected method/source mapping without state leakage.
- `python BuildTools/tests/test_docs_api.py`: 3 tests passed, covering deterministic overload IDs/source lines, independent inventory parity, and stable JSON metadata.
- `python BuildTools/tests/test_docs_validate.py`: 12 tests passed, including stale API-model rejection.
- `python BuildTools/docs_api.py --check`: current for methods, properties, events, and settings.
- `python BuildTools/docs_validate.py`: passed for all 50 maintained Markdown entries after API-model integration.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: production codegen, C++ build, resource bake, compatibility-version emission, both required script lifecycle markers, and clean server shutdown passed.

Follow-up:

- Generate human script/settings reference pages from `api.json` without restating signatures manually.
- Add source-authored stability/since/deprecation metadata before promoting any reachable symbol from `internal`.
- Integrate remote-call and other script-authored metadata through their owning parser, then add API diff classification.

## 2026-07-10 - generated native-codegen Markdown reference

Scope:

- `BuildTools/docs_reference.py` and `BuildTools/tests/test_docs_reference.py`.
- Seven generated pages under `Docs/generated/api/`, their manifest classifications, standalone freshness validation, and documentation CI commands.
- `PUBLIC_API.md`, `Docs/README.md`, `Docs/GeneratedApiAndMetadata.md`, `Docs/ScriptMethodsMap.md`, ADR 0002, and Phase 3 status routes.

Source areas checked:

- Every symbol kind and common field emitted by `BuildTools/docs_api.py`.
- Jekyll-compatible Markdown/front-matter requirements, stable symbol anchors, local navigation, and GitHub source-line links.
- The explicit distinction between command-line redaction-policy state and semantic credential sensitivity.

Results:

- Added a deterministic renderer that consumes only `Docs/generated/api.json`; it does not parse C++ or create a competing symbol model.
- Generated separate index, method, property, event, type/member, setting, and migration pages covering all 2,459 current native-codegen symbols.
- Added stable per-symbol anchors, normalized signatures, runtime sides, current stability labels, flags, source provenance links, and source-authored descriptions. Missing descriptions remain visible as the generated metadata-quality backlog.
- Classified all seven pages as public human reference with future English mirror destinations. The maintained Markdown inventory now contains 57 entries.
- Extended the manifest validator and fast documentation job so missing, stale, or manually edited generated pages fail byte-for-byte freshness checks.
- Kept `PUBLIC_API.md` as a visible placeholder because project-authored remote calls, CMake/CLI/package surfaces, explicit non-internal stability labels, and API-diff enforcement remain incomplete.

Mechanical checks:

- `python BuildTools/tests/test_docs_reference.py`: 5 tests passed, including every-symbol anchor coverage, Markdown/Liquid-safe escaping, unknown-kind rejection, deterministic output, and write/check behavior.
- `python BuildTools/tests/test_docs_validate.py`: 13 tests passed, including stale generated-page rejection.
- `python BuildTools/docs_api.py --check`, `python BuildTools/docs_reference.py --check`, and `python BuildTools/docs_inventory.py --check`: current.
- `python BuildTools/docs_validate.py`: passed for all 57 maintained Markdown entries.
- Python byte-compilation and `git diff --check`: passed.

Follow-up:

- Add source-authored stability/since/deprecation metadata and a reviewable classification backlog.
- Integrate project-authored remote calls through their owning structured parser.
- Add API snapshot diffing and explicit breaking-change disposition before replacing the public placeholder route.

## 2026-07-10 - source-owned API contract metadata

Scope:

- `BuildTools/codegen.py`, `BuildTools/docs_api.py`, `BuildTools/docs_reference.py`, and their focused tests.
- The first real `///@ ApiContract` source declaration in `Source/Scripting/CommonGlobalScriptMethods.cpp`.
- API schema v2, regenerated JSON/Markdown, ADR 0002, authoring guidance, and Phase 3 status.

Source areas checked:

- Existing codegen tag/meta/source stores, parser reset behavior, compatibility hashing, runtime generation loops, and export overload identity.
- ADR requirements for stable, experimental, internal, and deprecated surfaces, including since/replacement/removal policy.
- Generated page coverage for every symbol kind and local/HTTP example-link routing.

Results:

- Added the docs-only `ApiContract` tag to the normal typed codegen parser. It accepts exact symbol IDs or family IDs, stability and lifecycle fields, repeatable examples, and preceding contract notes while remaining outside the runtime compatibility hash.
- API schema v2 resolves selectors only after all canonical symbol IDs exist. Unknown and overlapping selectors, invalid examples, missing/self deprecated replacements, and incomplete lifecycle fields are hard generation errors.
- Every model symbol now records explicit/default contract provenance separately from declaration provenance. Summary fields expose declaration count, explicitly affected symbols, default-internal backlog, and explicit labels.
- Generated Markdown now renders API contract state, lifecycle, examples, notes, and contract source for methods, properties, events, settings, migrations, entities, types, and all type members.
- Explicitly classified `Game.BreakIntoDebugger` as development-only `internal`. The remaining 2,458 native-codegen symbols stay `internal (default)`; no stable/experimental promise was inferred from reachability or project usage.

Mechanical checks:

- Engine-only compatibility hash before and after adding `ApiContract`: `6586593177bf1e5f` in both runs.
- `python BuildTools/tests/test_docs_api.py`: 5 tests passed, including overload-family classification, deprecated replacement/lifecycle data, unknown-selector rejection, and hash invariance.
- `python BuildTools/tests/test_docs_reference.py`: 5 tests passed with every-symbol anchor and explicit contract rendering coverage.
- `python BuildTools/tests/test_docs_validate.py`: 14 tests passed, including explicit API schema-version pinning.
- API JSON, all seven reference pages, and source inventory freshness checks: current.
- `python BuildTools/docs_validate.py`: passed for all 57 maintained Markdown entries.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: normal project codegen/build/bake and script lifecycle smoke passed without compatibility drift.
- Python byte-compilation and `git diff --check`: passed.

Follow-up:

- Integrate project-authored remote calls through their owning structured parser without making engine docs depend on a game repository.
- Add API snapshot diffing and breaking-change disposition against explicit contract labels.
- Review non-internal classifications only with release/support policy and domain-owner approval; continue exposing default-internal and missing-description counts meanwhile.

## 2026-07-10 - project remote-call reference and baked catalog

Scope:

- `BuildTools/docs_metadata.py` and `BuildTools/tests/test_docs_metadata.py`.
- `Docs/RemoteCalls.md`, scripting/nullability/metadata routes, public entry points, manifest, CI, backlog, and Phase 3 status.
- `Examples/MinimalProject` declarations, both side-specific handlers, CMake smoke inputs, and runtime verifier.

Source areas checked:

- `MetadataBaker` remote-call grammar, target expansion, side direction, source-file hint, binary container framing, and focused parser failures.
- Dynamic metadata registration, inbound/outbound uniqueness, AngelScript caller registration, handler declaration resolution, attribute validation, serialization, and payload-end checks.
- The existing native `api.json` ownership boundary and the requirement that concrete game calls remain project-owned.

Results:

- Added a strict little-endian `.fometa` decoder and project catalog generator that consumes the owning parser's server/client bake outputs instead of reparsing `.fos`.
- The generator rejects truncated/trailing/invalid-UTF-8 data, malformed records, duplicate inputs/calls, signature/source mismatches, and unpaired production records. It emits deterministic JSON and GitHub Pages Markdown with `script.remote-call.<target>.<name>` IDs, caller/handler surfaces, nullable argument data, input hashes, and paired evidence.
- Added the engine-owned remote-call reference for declaration grammar, direction, namespace/file binding, supported runtime payload families, server authority, bounded payloads, compatibility, catalog generation, troubleshooting, and validation.
- Kept `api.json` scoped to engine-native codegen while documenting the baked project supplement as the owning representation for game-authored calls.
- Extended the minimal project with one call in each direction. The first full bake correctly rejected the missing client inbound implementation even with `FO_BUILD_CLIENT=0`; adding the matching `[[ClientRemoteCall]]` proved that baking validates both side contracts.
- Updated Phase 3 method/property/event/remote/enum/type reference coverage to complete. API diffing, remaining CMake/CLI/package domains, and broad owner-reviewed stability classification remain open.

Mechanical checks:

- `python BuildTools/tests/test_docs_metadata.py`: 4 tests passed, covering binary framing, direction pairing, nullability, malformed input, determinism, and CLI write/check behavior.
- `python BuildTools/tests/test_docs_api.py`: 5 tests passed.
- `python BuildTools/tests/test_docs_reference.py`: 5 tests passed.
- `python BuildTools/tests/test_docs_inventory.py`: 2 tests passed.
- `python BuildTools/tests/test_docs_validate.py`: 14 tests passed.
- Native API JSON, all seven generated reference pages, and source inventory freshness checks: current.
- `python BuildTools/docs_validate.py`: passed for all 58 maintained Markdown entries.
- Actual starter `.fometa-server/client` inputs produced and then passed `--check` for a two-call JSON/Markdown project catalog.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: production codegen/build/bake, both side handler bindings, runtime lifecycle, paired catalog IDs, and clean shutdown passed with compatibility version `23c3c0e2b71a1ed3`.
- Python byte-compilation for the generator, tests, and starter runner: passed.
- `git diff --check`: passed; Engine and parent-project staging areas are empty.

Follow-up:

- Add API snapshot diffing and require an explicit breaking-change disposition against source-owned contract labels.
- Generate CMake option/stage helper and BuildTools CLI/package references from their owning structured definitions.
- Confirm `linux-starter-smoke` and the Jekyll site artifact in GitHub Actions; administrator confirmation of the production Pages source remains external.

## 2026-07-11 - API diff and breaking-change disposition gate

Scope:

- `BuildTools/docs_api_diff.py`, its focused tests, and the cumulative disposition ledger.
- Documentation manifest validation and the GitHub Actions base-revision report/enforcement path.
- `Docs/ApiChangeManagement.md`, ADR 0002, generated API ownership docs, public/AI routes, backlog, and Phase 3 status.

Source areas checked:

- Canonical schema v2 symbol IDs, overload identity, stability/lifecycle fields, normalized signatures, parser contract, scope, descriptions, and provenance.
- Accepted stable/experimental/deprecated/internal policy and its migration/release/compatibility requirements.
- GitHub Actions pull-request base SHA and push `before` revision semantics, full-history checkout, always-uploaded diagnostics, and first-model bootstrap behavior.

Results:

- Added deterministic file/git revision comparison with separate full-model and provenance-insensitive contract digests. Source path/line churn cannot create a false breaking change or invalidate an otherwise identical contract disposition.
- Added additive, documentation, policy, and breaking classifications. Non-overloaded signatures modify one stable ID; signature-hashed overload changes are represented as removal/addition under the same family.
- Public enforcement uses baseline stability. Public removals/shape changes and `stable -> experimental/internal` or `experimental/deprecated -> internal` withdrawals require an exact disposition; internal refactors remain visible without becoming compatibility promises.
- Canonical source-parser, model-scope, and parser-contract changes always require disposition.
- Added a cumulative ledger whose entries bind change ID plus baseline/current contract hashes and require owner classification, rationale, migration, release-note, and compatibility handling. Old unmatched entries are inert rather than reusable approvals.
- GitHub Actions now compares the complete PR/push range, writes `Workspace/api-diff.json` and `.md`, blocks missing dispositions, and uploads the report even when enforcement fails. Standalone validation rejects removal of the ledger, full-history checkout, base-ref argument, or `--enforce`.
- Added the public maintainer guide and changed the Phase 3 API-diff item to complete. The overall public route remains a placeholder because non-codegen CMake/CLI/package domains and broad owner-reviewed non-internal classifications are still incomplete.

Mechanical checks:

- `python BuildTools/tests/test_docs_api_diff.py`: 7 tests passed, including public removal, signature change, stability-withdrawal bypass, overload replacement, parser-contract drift, stale digest, invalid ledger, and CLI artifact enforcement.
- `python BuildTools/tests/test_docs_validate.py`: 16 tests passed, including malformed ledger and missing workflow-enforcement failures.
- Existing API/reference/remote-metadata/inventory tests: 5 + 5 + 4 + 2 passed.
- Native API JSON, all seven generated reference pages, and source inventory freshness checks: current.
- `python BuildTools/docs_validate.py`: passed for all 59 maintained Markdown entries.
- Identical current/baseline models produced `pass` with zero changes; the real current `HEAD` produced the expected `bootstrap` report because this uncommitted documentation program has not yet placed `api.json` in git history.
- GitHub Actions YAML parsed successfully.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: production codegen/build/bake, runtime lifecycle, paired remote-call catalog verification, and clean shutdown passed with compatibility version `23c3c0e2b71a1ed3`.
- Python byte-compilation and `git diff --check`: passed; Engine and parent-project staging areas are empty.

Follow-up:

- Observe the first API-diff artifact after this model lands; that first base without `api.json` is the only expected bootstrap run.
- Generate CMake option/stage helper/hook reference from owning structured definitions, then cover BuildTools CLI/package surfaces.
- Obtain owner/release-policy review before promoting native symbols beyond `internal`; diff enforcement does not infer public promises.
- Confirm `linux-starter-smoke` and the Jekyll site artifact in GitHub Actions; administrator confirmation of the production Pages source remains external.

## 2026-07-11 - generated CMake project-interface reference

Scope:

- `BuildTools/cmake/ProjectInterface.json` as the runtime-consumed project option, stage/entrypoint/hook, and selected-helper contract.
- `BuildTools/docs_cmake.py`, its focused tests, the structural CMake test, canonical JSON model, and four generated Markdown pages.
- Documentation manifest/freshness validation, GitHub Actions commands, BuildTools/public/AI routes, backlog, and Phase 3 status.

Source areas checked:

- Existing option declarations and precedence in `BuildTools/cmake/stages/Init.cmake` and `BuildTools/cmake/helpers/Options.cmake`.
- Stage execution, ordering, hook registration, and public entrypoints in `BuildTools/Init.cmake` plus all ten current stage files.
- Project-facing helper definitions in `BuildTools/Init.cmake` and `BuildTools/cmake/helpers/Build.cmake`, including repeated role/path pairs for `AddEngineSources`.
- Engine-owned minimal-project CMake composition and the production `win64-starter-smoke` configure/build/bake/runtime path.

Results:

- Added a strict schema-1 project-interface manifest containing the current 43 options, ten ordered stages, and five selected embedding helpers. Each documentation record has a stable `cmake.option.*`, `cmake.stage.*`, or `cmake.helper.*` ID.
- CMake now reads that manifest during configure: stage entrypoints are generated from its ordered records, hook names are validated from it, published helpers must resolve to real commands, and the `Init` stage declares options from the same data rendered by documentation.
- Added deterministic `Docs/generated/cmake.json` plus index, option, stage/hook, and helper pages with defaults, required state, precedence, signatures, roles, responsibilities, scope/support status, and source links.
- Declared the surface `experimental` with no versioned support line. BuildTools CLI/package grammar and native-extension ABI remain separate unfinished domains; native API diffing does not yet compare `cmake.json`.
- Manifest validation and the fast documentation workflow now reject CMake schema, source-path, structural-command, and byte-for-byte generated-output drift. All four human pages are classified for the future English/Russian mirror.
- The first full smoke exposed an unquoted semicolon in a boolean option help string. `DeclareBoolOption` now quotes the complete description, and the structural test declares every manifest option so this class of failure is caught before a native build.
- Removed the configure-time `cmake-vars.txt` side effect by consuming `cmake --help-variable-list` through `OUTPUT_VARIABLE`; script-mode checks no longer dirty the checkout.

Mechanical checks:

- Focused documentation tests: 46 passed (`5` API, `7` API diff, `5` CMake, `5` native reference, `4` remote metadata, `2` inventory, `18` standalone validator).
- `cmake -P BuildTools/tests/validate_project_interface.cmake`: passed while declaring all 43 options and verifying ten entrypoints/two hook positions per stage/five helpers; no `cmake-vars.txt` was created.
- Native API JSON, CMake JSON/pages, seven native reference pages, and source inventory freshness checks: current.
- `python BuildTools/docs_validate.py`: passed for all 63 maintained Markdown entries.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: configure, native build, codegen, bake, both required lifecycle markers, baked remote-call catalog verification, and clean shutdown passed with compatibility version `23c3c0e2b71a1ed3`.
- Python byte-compilation and `git diff --check`: passed; Engine and parent-project staging areas are empty.

Follow-up:

- Generate a BuildTools CLI model/reference from owning `argparse` definitions and tested help output.
- Define and generate package declaration/payload contracts from the owning CMake/package parser instead of prose duplication.
- Extend revision comparison and disposition policy to the separate CMake and future CLI/package models before treating the complete project interface as stable.
- Confirm `linux-starter-smoke`, the Jekyll site artifact, and the first landed API-diff artifact in GitHub Actions; production Pages source confirmation remains external.

## 2026-07-12 - revision reconciliation and generated BuildTools CLI reference

Scope:

- Engine fast-forward `67ee893ae721d149cd44ff314abd8036adfd3821..411fbf09739a670125ddaaded1df2c4981f033e5`, integrated by embedding-project fast-forward `805caa799..f40bc2104` and its matching Engine gitlink.
- Revision-update ownership in `AGENTS.md` and `Docs/DocumentationMaintenance.md`, with a matching embedding-project reconciliation procedure maintained outside this engine repository.
- `BuildTools/buildtools.py::create_parser()`, new `BuildTools/docs_cli.py`, focused tests, canonical CLI JSON, two generated Markdown pages, manifest freshness validation, and documentation CI.
- Backlog, production-plan, public/API/build routes, and generated native/CMake cross-links.

Source areas checked:

- Every incoming Engine commit and the complete name/status diff, including source, tests, and owning docs rather than commit subjects alone.
- Location property broadcasts (`d49333973`), MSBuild test logging (`50245ab41`), removed helpers (`3a10b3549`), smart-pointer refactoring (`2eaf2f628`), finite numeric/property/layout and `BindFont` scale contracts (`6453772d6`), compile repairs (`26fdf21be`), guarded `nptr` dereference behavior (`d9f56e848`), property overlay alignment (`1c1314ae7`), Vulkan handle ownership (`c27171505`), and sanitizer/Direct3D/runtime exception fixes (`411fbf097`).
- `Source/Common/Common.h`, native metadata/codegen output, `ClientRuntime.md`, `ServerRuntime.md`, `EntityModel.md`, `Persistence.md`, `Essentials.md`, `Nullability.md`, `FrontendAndRendering.md`, `ExceptionSafety.md`, `Testing.md`, and `BuildToolsPipeline.md`.
- The actual BuildTools `argparse.ArgumentParser`, all command-specific executable `--help` paths, existing CI/starter consumers, and the standalone manifest/publication boundary.

Results:

- Added a mandatory revision reconciliation workflow: record old/new SHAs, retain safety copies, audit the full range, route each changed surface to an owning doc, regenerate affected contracts, compare old/current models, validate the embedding project, and drop the safety stash only after conflict/staging/freshness checks pass.
- Removed fixed compatibility-version examples from both engine and embedding-project maintainer instructions. The instructions now locate the current `MigrationRule Version` marker from source, preventing copied documentation from silently becoming stale.
- Reconciled the incoming runtime changes with their owning pages. Incoming documentation was retained where source-backed and supplemented for Location property broadcasts, MSBuild `RunAndLog.cmake`, and guard-aware direct `nptr<T>` dereference. Compile-only/helper-removal commits required no public behavior claim.
- Regenerated the native API model/reference against a preserved pre-update model. The exact delta contains two breaking-but-internal modifications: `migration.Version.0.0` now resolves to migration version 28, and `script.method.client.Game.BindFont` adds `float32 defaultScale = 1.0f`. Current policy requires no disposition for either change.
- Added a deterministic schema-1 `buildtools-cli` model generated by importing the executable `create_parser()` factory, not by parsing Python source or maintaining a second command list. It contains stable command/argument IDs, action/cardinality/choice/default/type data, exact usage and help output, source/scope metadata, and a contract digest.
- Added generated CLI index and command pages for all 11 commands and 22 arguments. Filling missing descriptions in `create_parser()` improved executable `--help` and generated reference together. The surface remains honestly classified `internal` until a versioned support policy is approved.
- Added byte-for-byte model/page freshness checks to the standalone validator and GitHub Actions. The focused test executes top-level help and all 11 `<command> --help` routes at a fixed width and proves generation remains identical under a different ambient terminal width.
- Updated generated native/CMake indexes, human navigation, manifest ownership, BuildTools docs, backlog, production plan, and public placeholder boundaries. Package contracts, helper-script CLIs, and multi-domain compatibility comparison remain open rather than being implied complete.

Mechanical checks:

- Focused documentation tests: 51 passed (`5` API, `7` API diff, `4` CLI, `5` CMake, `5` native reference, `4` remote metadata, `2` inventory, `19` standalone validator).
- Native API, CLI, CMake, native Markdown, and source-inventory `--check` commands: current. The inventory reports 947 export methods, 84 native test files, and 265 settings; the standalone manifest covers 65 Markdown entries.
- Preserved-baseline API diff: `pass`, two changes, zero required dispositions, zero missing dispositions.
- `cmake -P BuildTools/tests/validate_project_interface.cmake`: passed for 43 options, ten stages, and five selected helpers.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: clean configure/build/bake/runtime shutdown on Engine `411fbf097`; both lifecycle markers and baked remote-call metadata passed with compatibility version `3c1157d446f74afe`.
- Embedding-project `BakeResources`: clean full rebake on root `f40bc2104`, including script compilation and 612 maps, with project compatibility version `a74c943a85d389ee`.

Follow-up:

- Define and generate package declaration and payload contracts from their owning CMake/package parser.
- Extend revision comparison and disposition policy across native, CMake, CLI, and package models before promoting the complete project interface beyond revision-pinned status.
- Confirm the first landed API-diff, documentation-site, and `linux-starter-smoke` artifacts in GitHub Actions; production Pages source confirmation remains an administrator task.

## 2026-07-12 - revision reconciliation and generated package contract

Scope:

- Root fast-forward `f40bc2104..4a0c0efc6` and Engine fast-forward `411fbf097..bd6f7316c`, including restoration of both documentation worktrees and resolution of the incoming `Docs/ExceptionSafety.md` overlap.
- Runtime-consumed `BuildTools/PackageInterface.json`, `BuildTools/package.py`, `DefinePackage`, deterministic package JSON/Markdown generation, structural tests, standalone freshness validation, and documentation CI.
- Package/public/AI/maintenance routes, project build and architecture links, backlog, production plan, and current package claims in the embedding project.

Source areas checked:

- Incoming malformed pre-handshake logging and exception-safety commits, their native tests, and owning networking/exception documentation.
- `DefinePackage` parsing and command construction in CMake, executable `package.py` argument parsing, platform implementations, payload staging, resource modes, archive/install outputs, and the current Last Frontier package declarations.
- Current and historical Android package entries in the embedding project, including removal commit `39196acf9` and the remaining CI SDK/NDK setup.

Results:

- Reconciled incoming exception and networking behavior without making reusable engine guidance depend on Last Frontier's project-local analyzer. Removed the unsupported `archive/noexcept-sweep` tag claim and documented project baselines as non-normative.
- Compared the regenerated native API model with the preserved pre-update baseline: zero contract changes, zero required dispositions, and zero missing dispositions.
- Added one runtime-consumed package contract for five targets, six platforms, 19 pack tokens, six payload families, eight artifact-producing packs, and the 13-argument internal packager CLI. Stable `package.*` IDs feed a deterministic JSON model and five GitHub Pages-compatible reference pages.
- Fixed both documented `argparse` factories to declare stable program names. A combined test discovery run can no longer leak its own `sys.argv[0]` into generated BuildTools CLI or package help.
- `package.py` now rejects malformed pack/architecture lists, unsupported platforms, invalid target/platform/pack combinations, placeholder packs, missing target-required modifiers, and modifier-only requests before staging. `DefinePackage` now requires at least one `BINARY` clause.
- A real `MakePackage-Dev` run exposed repeated same-name runtime members in `SingleZip`. The archiver now stores byte-identical members once and rejects conflicting contents; the generated contract and regression test describe and enforce that behavior.
- Corrected stale Last Frontier claims that `Daily`, `Staging`, and `Prod` currently emit Android APKs. The current declarations produce no Android package; SDK/NDK preparation alone is not an APK input or artifact.
- Public navigation and manifest ownership now expose the generated package reference while keeping the embedding project's concrete package matrix outside the reusable engine contract.

Mechanical checks:

- Focused documentation tests: 57 passed (`5` API, `7` API diff, `4` CLI, `5` CMake, `5` package, `5` native reference, `4` remote metadata, `2` inventory, `20` standalone validator).
- Package implementation tests: 6 passed and one platform-specific WiX test skipped; both package and project-interface CMake structural checks passed.
- Native API, CLI, CMake, package, native Markdown, and source-inventory generated checks are current. Standalone validation covers 70 maintained Markdown entries.
- `python BuildTools/buildtools.py validate win64-starter-smoke`: configure, build, bake, runtime lifecycle, remote-call metadata, and clean shutdown passed on Engine `bd6f7316c` with compatibility version `3c1157d446f74afe`.
- Last Frontier `BakeResources` passed on root `4a0c0efc6` with compatibility version `a74c943a85d389ee`.
- Last Frontier `MakePackage-Dev` passed after building its declared Server, Client, ClientLib, Mapper, and BakerLib inputs. The resulting 232,451,650-byte package-wide ZIP contains ten unique members and no duplicate names or warnings.

Follow-up:

- Add a multi-domain revision comparison and disposition gate for native API, CMake, BuildTools CLI, and package models.
- Cover helper-script CLIs and project-authored configuration keys as separate owner-backed surfaces rather than extending the internal packager CLI beyond its source boundary.
- Confirm the first landed documentation-site/API-diff artifacts and `linux-starter-smoke` in GitHub Actions; production Pages source confirmation remains an administrator task.

## 2026-07-13 - resource-pack reconciliation and multi-domain contract diff

Scope:

- Root fast-forward `4a0c0efc6..255a94836` and Engine fast-forward `bd6f7316c..3c1b0d0a7`, including named safety preservation and conflict-free restoration of both documentation worktrees.
- Incoming resource-pack glob filtering, offscreen scissor behavior, project resource-pack migration, and their owning engine/project documentation.
- Native API, CMake, main BuildTools CLI, and package model comparison through one revision-pair report and shared disposition ledger.

Source areas checked:

- `FileSystem::FilterFiles`, `ResourcePackInfo`, `GlobalSettings::AddResourcePacks`, every baker/tool resource consumer, `SpriteManager` scissor handling, and the new FileSystem/Settings/Baker tests.
- Incoming `ConfigurationAndDataSources.md`, `FrontendAndRendering.md`, Engine maintainer rules, Last Frontier `LastFrontier.fomain`, and the stale project architecture table.
- All four canonical generated models, their source parsers/manifests, the specialized native API comparator, documentation manifest/validator, GitHub Actions workflow, and public change-management routes.

Results:

- Retained the complete incoming engine guidance for recursive resource mounting, case-sensitive include/exclude globs, separator normalization, precedence, examples, and offscreen scissor preservation. Corrected Last Frontier's stale `RecursiveInput` architecture claim and routed detailed semantics to the reusable engine page.
- Regenerated native API, CMake, CLI, package, native Markdown, and source inventory outputs after the revision update. All four canonical models are byte-identical to the preserved pre-update snapshots; both specialized API and aggregate reports contain zero changes and zero required dispositions.
- Added `BuildTools/docs_contract_diff.py`. The aggregate report delegates native symbols/overloads to `docs_api_diff.py` and flattens CMake/CLI/package records by their source-owned stable IDs. Nested documentation changes, policy changes, additions, removals, shape changes, source/scope changes, and domain stability are classified without raw JSON text matching.
- CMake remains `experimental`, so breaking entry changes block without disposition. Main CLI and package domains remain `internal`, so entry churn is visible but does not create an accidental compatibility promise. Model source/scope/contract changes always require review.
- Replaced the API-only ledger with schema-v2 `Docs/contract-change-dispositions.json`. Entries bind explicit domain, domain-prefixed change ID, and that domain's baseline/current contract digests; stale or cross-domain entries cannot satisfy the gate.
- Added partial Git bootstrap: a model absent from the selected base is visibly bootstrapped while already-landed domains are still compared. Unknown Git revisions remain errors, and local directory baselines must contain all four models.
- GitHub Actions now writes and always uploads `Workspace/contract-diff.json` and `.md`; standalone validation pins the four model paths, aggregate generator/test, shared ledger, full-history checkout, base ref, and enforcement switch.
- Updated change-management, ADR, public API route, BuildTools/AI/maintenance guidance, backlog, production plan, manifest ownership, and embedding-project update instructions. Helper-script CLIs, native-extension ABI, project-authored settings, and behavior behind unchanged declarations remain explicit future domains.

Mechanical checks:

- Combined documentation discovery: 65 tests passed (`57` previous focused/validator tests plus `8` aggregate contract-diff tests).
- All API/CMake/CLI/package/reference/inventory generators are current; standalone validation passes for 70 maintained Markdown entries.
- Both CMake structural tests passed. Package implementation tests passed 6 with one platform-specific WiX skip. Python byte-compilation passed for all documentation generators and the changed BuildTools entrypoints.
- Preserved-baseline aggregate diff: `pass`, four domains, zero changes, zero required dispositions, zero missing dispositions. Specialized native API diff reports the same zero delta.
- `RunUnitTests`: all 334 test cases and 355,687 assertions passed, including the incoming resource-pack/FileSystem/Settings/Baker coverage.
- `win64-starter-smoke`: configure, build, bake, paired remote-call metadata, lifecycle markers, and clean shutdown passed on Engine `3c1b0d0a7` with compatibility version `3c1157d446f74afe`.
- Last Frontier `BakeResources`: clean full rebuild on root `255a94836`, project version `0.3.512`, compatibility version `a74c943a85d389ee`, including script compilation and 612 maps under the new glob-filter contract.
- Final root/Engine `git diff --check`, conflict-marker, JSON/YAML parsing, staging, gitlink, and upstream checks passed. Both branches report `0 0` against upstream; only the two named safety stashes were removed, leaving older stashes untouched.

Follow-up:

- Model helper-script CLIs by owner and user impact instead of folding unrelated parsers into the main BuildTools CLI model.
- Define native-extension ABI coverage only after the support boundary and release policy are owner-approved.
- Confirm the first landed aggregate contract-diff/site artifacts and `linux-starter-smoke` in GitHub Actions; production Pages source confirmation remains an administrator task.

## 2026-07-13 - generated helper CLI reference

Scope:

- Every engine-owned Python helper with a top-level executable `create_parser()` outside the separately modeled main BuildTools and package command lines.
- Runtime parser ownership, deterministic JSON/Markdown generation, complete parser discovery, standalone freshness validation, documentation CI, and aggregate contract comparison.
- Codegen, Mono compilation, coverage, Android-device, local-web-server, and MSI invocation owners plus the embedding-project update-reconciliation route.

Source areas checked:

- `BuildTools/codegen.py`, `compile-mono-scripts.py`, `codecoverage.py`, `android_device.py`, `web/simple-web-server.py`, and `msicreator/createmsi.py` parser/runtime paths.
- CMake codegen, scripts/baking, coverage targets, package WebServer/Wix consumers, current platform/build prose, and focused BuildTools/package tests.
- `BuildTools/docs_cli.py`, the new helper manifest/generator/tests, aggregate comparator/disposition validation, documentation manifest/validator, GitHub Actions, ADR, public/AI routes, backlog, and production plan.

Results:

- Added runtime-owned `create_parser()` factories with stable program names for all six helpers. Coverage now exposes documented `clean`, `run`, `report`, and `full` subcommands; MSI execution and generated help use the same parser while preserving the existing `run(list[str])` fixture boundary and bare-filename validation.
- Added `BuildTools/HelperCliInterface.json` for stable helper identity, owner, audiences, invocation owner, and explicit main-CLI/package exclusions. `BuildTools/docs_helper_cli.py` AST-scans the BuildTools tree and fails when a new parser is neither modeled nor assigned to another canonical domain.
- Generated [generated/helper-cli.json](generated/helper-cli.json) plus checked index/command pages for 6 helpers, 11 subcommands, 17 global arguments, and 35 subcommand arguments. Every helper/command/argument has a stable `helper-cli.*` ID and exact fixed-width executable help.
- Added helper CLI as the fifth aggregate contract domain. It remains `internal` and revision-pinned: shape/ownership changes are visible, but ordinary entry churn does not create an accidental compatibility promise. Model source/scope/contract changes retain mandatory disposition handling.
- Added model/page freshness and workflow checks to standalone validation and CI. Human, AI, public-API, maintenance, change-policy, backlog, and production-plan routes now point to the helper reference and identify native-extension ABI as the next local contract gap.
- Fixed the CMake Mono invocation to pass the parser-required `FO_OUTPUT_PATH` scripts/project directory. Updated stale codegen pointer fixtures to the current `ptr`/`nptr` ABI and isolated the WiX package test from its global subprocess monkeypatch.

Mechanical checks:

- Combined documentation discovery: 70 tests passed, including 4 helper CLI tests and 21 standalone validator tests. Every real helper/subcommand `--help` path, AST inventory, deterministic rendering, escaping, and stale detection passed.
- API/CMake/main-CLI/helper-CLI/package/reference/inventory generators are current; standalone documentation validation passes for 72 maintained Markdown entries.
- Both CMake structural tests passed. Focused codegen/package pytest passed 11 tests with one platform-specific skip. Python byte-compilation and JSON/YAML parsing passed.
- Aggregate Git-baseline report: visible first-landing bootstrap for the new domain, 5 domains, zero changes, zero required dispositions, and zero missing dispositions.
- `win64-starter-smoke`: configure, build, codegen, bake, paired remote-call metadata, lifecycle markers, and clean shutdown passed on Engine `3c1b0d0a7` with compatibility version `3c1157d446f74afe`.
- Last Frontier `BakeResources`: codegen and incremental bake passed on root `255a94836`, project version `0.3.512`, compatibility version `a74c943a85d389ee`, with no warnings or errors.
- Final root/Engine fetch, `git diff --check`, conflict, staging, branch, and gitlink checks passed. Both repositories report `0 0` against upstream; the root gitlink and Engine HEAD are `3c1b0d0a7`; the four older user stashes remain untouched.
- Local Jekyll rendering was not available because this Windows environment has no Ruby/Bundler. The repository pins remain machine-validated and the GitHub Actions `jekyll-build-pages` artifact job is still the publication render gate.

Follow-up:

- Native-extension ABI ownership/model coverage is completed in the next section without promoting project-local extension behavior to an engine guarantee.
- Confirm the first landed six-domain contract-diff, documentation-site, and `linux-starter-smoke` artifacts in GitHub Actions; production Pages source confirmation remains an administrator task.

## 2026-07-13 - native-extension interface and conformance

Scope:

- Project-native C++ source roles, engine hooks, generated fallbacks, script exports, binding/lifetime/dependency rules, revision compatibility, and executable conformance.
- Runtime CMake/codegen ownership, deterministic JSON/Markdown reference, sixth-domain change policy, human/AI navigation, update reconciliation, and the engine-owned minimal project.

Source areas checked:

- `BuildTools/Init.cmake`, `BuildTools/cmake/ProjectInterface.json`, role routing/core libraries/stages, `BuildTools/codegen.py`, and all ten hook call sites under `Source/`.
- The new runtime manifest/generator/tests, aggregate comparator, documentation validator/manifest/workflow, public API/ADR/maintenance routes, backlog, production plan, and minimal project.

Results:

- Added runtime-consumed `BuildTools/NativeExtensionInterface.json` with five roles, ten hooks, six binding rules, stable `native-extension.*` IDs, explicit `experimental` revision-pinned policy, and no cross-revision binary compatibility promise.
- CMake loads the manifest, verifies exact role parity with `ProjectInterface.json`, and rejects unknown `AddEngineSources` roles. `codegen.py` now derives hook recognition, compatibility-hash participation, declarations, and fallback definitions from the same manifest.
- Added [NativeExtensions.md](NativeExtensions.md), [generated/native-extension.json](generated/native-extension.json), and four checked reference pages. The guide covers composition order, role selection, namespaces/exports, hook fallbacks/lifecycle, instance-owned state, dependencies/platform guards, secrets, update reconciliation, and validation.
- Added native extensions as the sixth aggregate domain. Experimental role/hook/binding removals or shape changes require exact disposition; project implementations, SDKs, settings, persistence, packaging, and release policy remain outside the engine guarantee.
- Expanded the minimal project with `Server_Game_NativeStarterValue`, a generated `Game.NativeStarterValue()` call, and required runtime marker `starter_native_extension_value=42`; the existing visibility hook still proves fallback suppression.

Mechanical checks:

- Combined documentation discovery: 76 tests passed, including 5 native-extension tests and 22 standalone-validator tests. All six generated models are current; standalone validation passes for 77 maintained Markdown entries.
- Three structural CMake tests passed, including valid role routing/header behavior, role-manifest parity, and expected rejection of unknown `EDITOR`. Focused codegen/package pytest passed 11 tests with one platform-specific skip.
- Aggregate Git-baseline report: visible first-landing bootstrap for the sixth domain, zero changes, zero required dispositions, and zero missing dispositions.
- `win64-starter-smoke`: configure, native compile/link, codegen, bake, `starter_native_extension_value=42`, lifecycle markers, paired remote-call metadata, and clean shutdown passed with compatibility version `9112a846dd71cc41`.
- `RunUnitTests`: all 334 test cases and 355,687 assertions passed with project compatibility version `a74c943a85d389ee`.
- Last Frontier `BakeResources`: codegen and incremental bake passed for project version `0.3.512` with no warnings or errors.
- Final root/Engine fetch found no incoming commits. Root `255a948368bbe57745571828965997cf395ff3c0` and Engine `3c1b0d0a78042fcecdb4f29904c1efd46bed1102` each report `0 0` against upstream; the root gitlink matches Engine HEAD, staging and unmerged counts are zero, and the four older user stashes remain untouched.
- Local Jekyll rendering remains unavailable because this Windows host has no Ruby/Bundler; GitHub Actions `jekyll-build-pages` remains the publication render gate.

Follow-up:

- Publish the separate `fonline-native-extension-sample` only after the in-tree contract is reviewed and tagged; keep the engine-owned minimal project as the CI conformance source.
- Confirm the first landed six-domain contract-diff, documentation-site, and `linux-starter-smoke` artifacts in GitHub Actions; production Pages source confirmation remains an administrator task.

## 2026-07-13 - script lifecycle and concurrency guide

Scope:

- Reusable AngelScript module initialization, callback ownership, async propagation, suspension/resumption, server entity synchronization, mutable-state ownership, destruction, and shutdown.
- Root update from `255a948368bbe57745571828965997cf395ff3c0` to `035b3068d8670d5a275d64f7ea500f1d489dafcc`; Engine remained at `3c1b0d0a78042fcecdb4f29904c1efd46bed1102`.
- Human/AI routing, translation classification, focused source-backed validation, documentation CI, and embedding-project ownership boundaries.

Source areas checked:

- `ScriptSystem` init ordering/global freeze, AngelScript attribute validation and backend function indexing, context suspension/resumption, client scheduled-callback snapshots, and server worker/script sync-context scopes.
- `EntitySync` replacement covers and singleton buckets, script-visible `Game.Sync` / `Game.SyncRelease` / `Game.Lock`, entity event/time-event cleanup, and focused engine tests.
- Incoming Last Frontier feedback/localization/test changes, including the move from shared id-keyed footstep cadence state to entity-owned non-persistent state; project code was research evidence only, not a normative engine dependency.
- The current public TLA project layout and scripting guidance as a non-normative research input; untagged project helpers were not promoted into engine guarantees.

Results:

- Added [ScriptLifecycleAndConcurrency.md](ScriptLifecycleAndConcurrency.md). It defines the runtime as bounded script entries and makes the suspension rule explicit: server covers, singleton locks, and mutable-state decisions do not survive `Yield`; continuations must re-resolve/revalidate, reacquire, and re-read.
- Documented stable ascending `[[ModuleInit]]` priorities, the global freeze boundary, callback-only attribute ownership, transitive `[[Async]]`, client next-pass `Yield(0)`, server worker-pool resumption, complete-set `Game.Sync` replacement, the `Game.Lock` ordering constraint, and entity-owned state guidance.
- Kept Last Frontier's module topology in its project `Docs/Scripts.md` and routed reusable semantics to the engine guide. Updated engine human/AI indexes, scripting/test routes, root maintainer routing, and the translation-required manifest target.
- Added `test_docs_script_lifecycle.py` with four source-backed checks. `docs_validate.py` now requires the focused test in the GitHub documentation workflow, and validator regression coverage proves omission fails.

Mechanical checks:

- Focused lifecycle documentation tests: 4 passed.
- Standalone validator tests: 23 passed.
- Complete documentation discovery: 81 tests passed; standalone validation covers 78 maintained Markdown entries; documentation-manifest JSON, disposition JSON, GitHub Actions YAML, Jekyll config YAML, links, anchors, source paths, and generated freshness checks passed.
- Last Frontier `BakeResources`: clean full rebuild on root `035b3068d`, project version `0.3.513`, compatibility version `a74c943a85d389ee`; AngelScript compilation and all 612 maps completed without warnings or errors.
- Final fetch reports root `035b3068d` and Engine `3c1b0d0a7` each `0 0` against upstream; the root gitlink matches Engine HEAD, staging and unmerged counts are zero, and `git diff --check` passes. The task safety stash was removed after validation; two older root stashes and two older Engine stashes remain untouched.
- Local Jekyll rendering remains unavailable because Ruby/Bundler is not installed on this Windows host; GitHub Actions `jekyll-build-pages` remains the publication render gate.

Follow-up:

- Build the next local production slice as an engine-owned authored-format model/reference from its parser or baker.
- Confirm the first landed documentation-site, six-domain contract-diff, and `linux-starter-smoke` artifacts; production Pages source confirmation remains administrator work.

## 2026-07-13 - prototype format reference and update reconciliation

Scope:

- Safe root update from `035b3068d8670d5a275d64f7ea500f1d489dafcc` to `34d36e7017b05ecd2f546f0d67819940277156af` and Engine update from `3c1b0d0a78042fcecdb4f29904c1efd46bed1102` to `06b0ef451be87fb94080af8307f633921c285ba2`, preserving the existing dirty documentation program and generated artifacts.
- The first engine-owned authored-content contract: prototype discovery, section grammar, identity, inheritance, property applicability and values, references, migrations, diagnostics, and baker side outputs.
- Deterministic machine reference, GitHub Pages-compatible human guidance, seventh-domain evolution policy, embedding-project routing, and update-maintenance obligations.

Source areas checked:

- `Source/Tools/Baker/ProtoBaker.cpp`, `Source/Common/ConfigFile.cpp`, `Source/Common/Properties.cpp`, property serialization and migration paths, `Settings.inc`, API-model metadata, and focused engine tests.
- Incoming script context lifetime, inbound remote-call synchronization, `Game.OnCritterPreLoad`, updater documentation, and the five-test `ScriptLifecycleAndConcurrency.md` source contract.
- Last Frontier prototype catalogs, project-required `$Name` policy, content routes, maintenance workflow, and full resource bake. Project conventions were retained as companion guidance rather than promoted to engine guarantees.

Results:

- Added `BuildTools/PrototypeFormatInterface.json`, `docs_prototype_format.py`, and five focused tests. The generated model and four Markdown pages expose 4 prototype types, 113 live properties, 92 parser-authorable properties, 21 excluded temporary/core properties, and 11 source-backed format and validation rules.
- Added [PrototypeFormat.md](PrototypeFormat.md) as the reusable authoring guide. It distinguishes engine basename fallback from stricter project identity policy, explains deterministic parent-before-component inheritance, rejects cycles, documents strict scalar/container values and references, and routes project semantic catalogs back to the embedding game.
- Integrated `prototype-format` as the seventh aggregate contract domain with stable IDs, source provenance, manifest ownership, freshness validation, CI execution, and experimental breaking-change disposition rules.
- Updated human, AI, public API, baking, entity, generated-metadata, maintenance, backlog, production-plan, and Last Frontier routes. Prototype parser/property-metadata changes now explicitly trigger same-change regeneration, review, and project bake.
- Reconciled the incoming API model at 2,460 symbols: 947 methods, 133 properties, 121 events, and 265 settings. `script.event.server.Game.OnCritterPreLoad` is recorded as the sole additive baseline change; 2,459 symbols remain default-internal and one contract is explicit.

Mechanical checks:

- Focused lifecycle tests: 5 passed. Focused prototype-format tests: 5 passed. Complete `test_docs*.py` discovery: 88 passed. Standalone validator tests: 24 passed; `docs_validate.py` validates 83 maintained Markdown entries.
- All API/reference/inventory/CMake/main CLI/helper CLI/native-extension/prototype-format/package generators are current. Three structural CMake interface tests passed; JSON parsing and `git diff --check` passed.
- Preserved-baseline aggregate diff: visible bootstrap for `prototype-format`, 7 domains, one additive change (`script.event.server.Game.OnCritterPreLoad`), zero required dispositions, and zero missing dispositions.
- `win64-starter-smoke`: Release configure/build, codegen, bake, native-extension value `42`, script lifecycle markers, paired remote-call metadata, server startup, and clean shutdown passed on Engine `06b0ef451` with compatibility version `6aaf98cf04f2acbd`.
- Last Frontier `BakeResources`: clean full rebuild on root `34d36e701`, project version `0.3.521`, compatibility version `83c5b2872ccdcf5c`; scripts, prototypes, texts, and all 612 maps completed without warnings or errors.
- Final fetch reports root and Engine `0 0` against upstream. The root gitlink and Engine HEAD both resolve to `06b0ef451`; staging and unmerged sets are empty, and only this task's two named safety stashes are removed after validation while four older user stashes remain untouched.
- Local Jekyll rendering remains unavailable because this Windows host has no Ruby/Bundler. GitHub Actions `jekyll-build-pages` remains the publication render gate for the Markdown site.

Follow-up:

- Model the complete `.fomap` contract beyond its prototype-like `[Header]`, including tile/object records, coordinates, ownership, diagnostics, and baker/runtime interpretation.
- Confirm the first landed seven-domain contract-diff, documentation-site, and `linux-starter-smoke` artifacts in GitHub Actions; production Pages source confirmation remains administrator work.

## 2026-07-14 - map format reference and strict current grammar

Scope:

- Safe root update from `34d36e7017b05ecd2f546f0d67819940277156af` to `a3dc2b77d2c7ddd085b39b4b4952f49aedffff70` and Engine update from `06b0ef451be87fb94080af8307f633921c285ba2` to `c523569b6232ac4f672612aea99ef06e97aa97b9`, preserving dirty documentation work and unrelated stashes.
- Reusable `.fomap` source syntax, map and placement identity, ownership references, property overrides, mapper round-trip, server/client baking, static/dynamic behavior, bounds, and runtime materialization.
- Deterministic JSON/Markdown reference, eighth-domain change policy, human/AI routing, update triggers, and Last Frontier integration boundaries.

Source areas checked:

- `Source/Common/ConfigFile.cpp`, `MapLoader.cpp`, metadata/property declarations, `Source/Tools/ProtoBaker.cpp`, `ProtoTextBaker.cpp`, `MapBaker.cpp`, mapper load/save, client static-map loading, server static-map loading/materialization, and focused map tests.
- Incoming root/Engine source, test, generated-model, and documentation changes across the complete recorded SHA ranges; Last Frontier maps and map-authoring tools were integration evidence, not normative engine dependencies.
- Existing prototype-format, contract-diff, documentation manifest/validator/workflow, public routes, maintenance policy, backlog, production plan, and publication-compatible Markdown structure.

Results:

- Corrected a real source/documentation mismatch. Current mapper, authored maps, and loaders use `[ProtoMap]`, but `ProtoBaker` retained a legacy `[Header]` branch and `MapBaker::ResolveMapName` read `$Name` from that obsolete section. The parser now requires exactly one first `[ProtoMap]`, accepts only `[Critter]` and `[Item]` afterward, rejects legacy/unknown sections, and names both baked outputs from `ProtoMap/$Name` or the source basename.
- Added focused unit coverage for first-section/cardinality/closed-section rules, current ProtoMap prototype handling, and explicit canonical baked output naming.
- Added `BuildTools/MapFormatInterface.json`, `BuildTools/docs_map_format.py`, five focused generator tests, [MapFormat.md](MapFormat.md), [generated/map-format.json](generated/map-format.json), and five checked reference pages. The model exposes 3 sections, 5 directives, 4 ownership modes, 16 rules, and 108 current Map/Critter/Item properties, including 87 authorable entries.
- Integrated `map-format` as the eighth aggregate contract domain. Fixed the shared disposition validator so `prototype-format` and `map-format` change IDs can be recorded, and added regression coverage that resolves experimental breaks across both domains.
- Added exact dispositions for replacing the erroneous experimental `[Header]` entry with `[ProtoMap]`; current projects already on `[ProtoMap]` require no content migration, while legacy content must convert and rebake both sides.
- Routed reusable map mechanics through engine docs and retained Last Frontier piece catalogs, composition grammar, AI authoring tools, custom metadata, and semantic validation in project docs.

Mechanical checks:

- Focused map-format tests: 5 passed. Complete `test_docs*.py` discovery: 94 passed. Standalone validator tests: 25 passed; `docs_validate.py` validates 89 maintained Markdown entries.
- All API/reference/inventory/CMake/main CLI/helper CLI/native-extension/prototype-format/map-format/package generators are current. Three structural CMake interface tests passed.
- Preserved-baseline aggregate diff: 4 changes across 8 domains, visible `map-format` bootstrap, 2 required dispositions satisfied, and 0 missing dispositions.
- Exception-safety audit: 5,259 functions checked, 0 errors, and 0 warnings after re-deriving unchanged `Basic`, `Strong`, and `Basic` levels for `MapLoader::Load`, `MapBaker::ResolveMapName`, and `ProtoBaker::BakeProtoFiles`.
- `RunUnitTests`: all 340 test cases and 355,901 assertions passed.
- Last Frontier `BakeResources`: clean full rebuild for project version `0.3.528`, compatibility version `b2418f8f43331b44`; scripts, prototypes, texts, and all 612 maps baked without warnings or errors.
- Final fetch reports root and Engine `0 0` against upstream. Root HEAD is `a3dc2b77d2c7ddd085b39b4b4952f49aedffff70`; root gitlink, upstream gitlink, Engine HEAD, and Engine upstream are `c523569b6232ac4f672612aea99ef06e97aa97b9`. Staging and unmerged sets are empty. Only the two named map-format safety stashes were removed after validation; four older user stashes remain untouched.
- Starter smoke was not rerun because the minimal-project/build surface did not change. Local Jekyll rendering remains unavailable because Ruby/Bundler is not installed; GitHub Actions `jekyll-build-pages` remains the publication render gate.

Follow-up:

- Select the next authored-format model from a confirmed parser/baker coverage gap; do not recreate Last Frontier-only conventions in engine docs.
- Confirm the first landed eight-domain contract-diff, documentation-site, and `linux-starter-smoke` artifacts; production Pages source confirmation remains administrator work.

## 2026-07-15 - manifest-backed AI documentation delivery

Scope:

- Safe Last Frontier update from `a3dc2b77d2c7ddd085b39b4b4952f49aedffff70` to `fd49c9583f5dcf9438b2d06a2adb5ed87e20ae79`; Engine remained at `c523569b6232ac4f672612aea99ef06e97aa97b9`.
- Deterministic `llms.txt`, bounded `llms-full.txt`, and public `docs-manifest.json` delivery from the canonical documentation manifest and Markdown corpus.
- GitHub Pages/Jekyll publication, stable URLs, normalized content hashes, visibility filtering, AI routing, maintenance triggers, CI freshness, and embedding-project reconciliation.

Source areas checked:

- `Docs/documentation-manifest.json`, public/current documentation entries, generated reference indexes, existing publication decisions, Jekyll configuration, documentation validators, and GitHub Actions.
- The new AI-delivery generator, focused tests, source-backed manifest and artifact validation, human/AI entry routes, maintenance guidance, backlog, production plan, and ADR 0003.
- The incoming Last Frontier `GlobalMap.CombatLocationProto` setting, its project documentation, runtime consumers, and project-owned resource-setting gameplay test. No reusable Engine contract changed in the incoming range.

Results:

- Added `BuildTools/docs_ai_delivery.py` and ADR 0003. The source manifest now owns the canonical locale, source ref, curated starting IDs, generated-page policy, and hard 1 MiB context budget.
- Generated root [llms.txt](../llms.txt) with all public current documentation routes, [llms-full.txt](../llms-full.txt) with public current authored pages plus generated indexes only, and [docs-manifest.json](../docs-manifest.json) with stable IDs, audiences, Diataxis type, ownership/state/stability, canonical/site/source URLs, normalized SHA-256 hashes, sizes, and artifact metadata.
- The full-context artifact is assembled from whole documents only and fails rather than truncating content when the budget is exceeded. Placeholder, internal, and generated detail pages are excluded according to the recorded policy.
- The three files are ordinary repository-root static artifacts, so the existing GitHub Pages/Jekyll deployment serves them at stable `fonline.ru` URLs without a separate renderer or documentation source.
- Last Frontier maintenance guidance now requires same-change reconciliation and generator checks when an Engine update changes inventoried Markdown, manifest metadata, public paths, generated models, or publication policy.

Mechanical checks:

- Focused AI-delivery tests: 5 passed. Standalone validator tests: 26 passed. Complete `test_docs*.py` discovery: 100 passed; `docs_validate.py` validates 90 maintained Markdown entries.
- All API, CMake, main CLI, helper CLI, native-extension, prototype-format, map-format, package, reference, inventory, and AI-delivery generated outputs are current. The public catalog contains 85 documents and `llms-full.txt` is 851,160 bytes against the 1,048,576-byte hard limit.
- Three structural CMake interface tests passed. Preserved-baseline aggregate diff remains at 4 changes across 8 domains, with 2 required dispositions satisfied and 0 missing.
- Last Frontier `BakeResources` completed a clean full rebuild for project version `0.3.529`, compatibility version `b2418f8f43331b44`; scripts and all 612 maps baked without warnings or errors.
- The first focused gameplay invocation exposed only a stale `LF_ServerHeadless` binary with old native bindings. Rebuilding that exact target completed without warnings; `resources.script_tunable_settings_are_valid` then passed 1/1 with zero failures, timeouts, skips, or global exception delta.
- Local Ruby/Bundler is unavailable on this Windows host. GitHub Actions `jekyll-build-pages` remains the authoritative render and publication gate; production Pages source confirmation remains administrator work.
- Final fetch found no new root or Engine commits. Root and Engine report upstream parity, the root gitlink matches Engine HEAD, staging and unmerged sets are empty, and `git diff --check` passes. Only the two named task safety stashes were removed after validation; four older user stashes remain untouched.

Follow-up:

- Confirm the first landed AI-delivery and `jekyll-build-pages` artifacts, then verify `https://fonline.ru/llms.txt`, `https://fonline.ru/llms-full.txt`, and `https://fonline.ru/docs-manifest.json` from the production deployment.
- Continue the production program with professional site navigation/search and clean Markdown endpoints, followed by reviewed public example repositories; freeze the English information architecture before introducing maintained `Docs/en` and `Docs/ru` trees.

## 2026-07-15 - manifest-backed site navigation and search

Scope:

- Safe continuation from Last Frontier `fd49c9583f5dcf9438b2d06a2adb5ed87e20ae79` and Engine `c523569b6232ac4f672612aea99ef06e97aa97b9`, with named root and Engine `include-untracked` safety stashes around the dirty documentation program.
- Manifest-owned information architecture, deterministic navigation/search data, a custom GitHub Pages/Jekyll reader shell, responsive interaction, and static search without changing Markdown ownership or the existing publication architecture.
- A final fetch discovered Last Frontier `fc098abd8faf2ecbb669a607959c8c65f725fd61`. The complete incoming range was audited and reconciled: it changes quest content, quest/encounter tests, AiControl playtest tooling, and project-owned quest/bag documentation, but not the Engine gitlink, reusable Engine contracts, the Engine documentation manifest, or the site-delivery policy. Engine remained at `c523569b6232ac4f672612aea99ef06e97aa97b9`.

Results:

- Added `BuildTools/docs_site.py` and ADR 0004. `Docs/documentation-manifest.json` now owns the site title/description, layout and asset contract, search policy, eight navigation groups, and exact top-level document placement.
- Generated `_data/docs-site.json` with 59 navigation items and `assets/docs-search.json` with 84 public current human documents in 638,290 bytes. The compact weighted index preserves technical identifiers and remains below its 1 MiB hard limit.
- Added `_layouts/default.html`, local CSS/JavaScript, and an engine-owned bitmap mark. The shell provides a persistent desktop sidebar, responsive drawer, current-page state, page-local table of contents, static search, source route, copy controls, rolling `master` indicator, and persisted light/dark theme without remote application dependencies.
- Extended AI delivery so `llms.txt` and public `docs-manifest.json` expose the generated site navigation and search artifacts with normalized hashes. The site generator runs before AI delivery, and standalone validation plus GitHub Actions enforce both outputs in that dependency order.
- Updated publication, maintenance, generated-metadata, navigation, production-plan, backlog, AI-maintainer, and Last Frontier integration guidance. Same-change maintenance now requires assigning eligible pages to manifest navigation and regenerating site data before AI-delivery artifacts.

Mechanical checks:

- Focused site generator tests: 5 passed. Focused layout contract tests: 5 passed. Complete `test_docs*.py` discovery: 111 passed. Standalone validator tests are included in that discovery; `docs_validate.py` validates 91 maintained Markdown entries.
- All API, CMake, main CLI, helper CLI, native-extension, prototype-format, map-format, package, reference, inventory, site, and AI-delivery generated outputs are current. The AI catalog contains 86 public documents and 864,863 full-context bytes.
- Three structural CMake interface tests passed. Preserved-baseline aggregate diff remains at 4 changes across 8 domains, with 2 required dispositions satisfied and 0 missing.
- Browser checks exercised a 1440 x 1000 desktop layout, table of contents, theme preference, and technical `Game.Sync` search, then a 390 x 844 mobile viewport. Mobile document width and scroll width both measured 390 px, the long heading wrapped within 354 px, and the animated drawer settled from x=0 through x=292 without overlap or browser exceptions.
- Ruby, Bundler, Docker, and Podman remain unavailable locally, so the browser pass used a static behavioral preview of the authored layout, CSS, and JavaScript. GitHub Actions `jekyll-build-pages` remains the authoritative Liquid render and publication gate.
- This slice changed documentation/site tooling only. It did not change Engine runtime/build contracts or Last Frontier gameplay behavior, so `RunUnitTests` and `BakeResources` were not rerun for this slice.
- Final fetches report root and Engine `0 0` against upstream. Root HEAD is `fc098abd8faf2ecbb669a607959c8c65f725fd61`; root gitlink, upstream gitlink, Engine HEAD, and Engine upstream are `c523569b6232ac4f672612aea99ef06e97aa97b9`. Staging and unmerged sets are empty, `git diff --check` passes, the four task-created safety stashes were removed after validation, and the four older user stashes remain untouched.

Follow-up:

- Confirm the first landed site-data and `jekyll-build-pages` artifacts, verify production navigation/search and the public AI endpoints on `fonline.ru`, and confirm the repository Pages source setting.
- Continue with ownership, support policy, shared template, and CI contract for reviewed public example repositories before creating the repositories themselves.

## 2026-07-15 - public example repository governance and template

Scope:

- Safe Last Frontier update from `fc098abd8faf2ecbb669a607959c8c65f725fd61` to `aef3174067dc1812a62b15d5fb8f04e3db18d1ce` and Engine update from `c523569b6232ac4f672612aea99ef06e97aa97b9` to `1bcf6e101a25533f701cc4a65fdfe93fe0de5bee`, preserving the dirty documentation program and unrelated stashes.
- Final reconciliation advanced Last Frontier again to `ac841fd79` through four project commits and Engine to `2f4fc0adf` through one test-only commit, with named include-untracked safety stashes around both dirty worktrees.
- A professional, owner-gated public-example program for `fonline-project-template`, `fonline-minimal-multiplayer`, `fonline-content-showcase`, and `fonline-native-extension-sample` before any external repository is created.
- Exact Engine pinning, scheduled current-Engine compatibility, repository governance, release evidence, updater/ABI boundaries, support/security ownership, and asset provenance.

Source areas checked:

- `Examples/MinimalProject`, the public project-composition CMake surface, existing starter smoke, documentation/site/AI manifests, maintenance policy, GitHub validation workflow, and generated-contract machinery.
- The complete incoming root and Engine ranges. Project maps, gameplay, analytics, tests, MapAuthor/AiControl tooling, CI, and project docs remained project-owned. The reusable Engine range changed the updater protocol from generation 1 to 2, client host/runtime ABI from 2 to 3, and compatibility migration from 29 to 30; its updater guide and focused native/integration tests were preserved.
- The final root range updates quest/gameplay synchronization, synchronization-audit contracts and tests, project-owned quest/faction/global-map documentation, and the Engine gitlink. The final Engine range only supplies explicit types for integration-test port and response-encryption-key values; it does not change reusable runtime behavior or require a new Engine documentation contract.
- The refreshed Last Frontier bake exposed an incoming test compile error: `Sync::Snapshot()` returns base `Entity` handles, so `Test_Factions.fos` could not read `restoredCover[0].Id`. The exact-cover regression now compares handles directly, using the engine's identity fallback after entity `opEquals` removal.
- The proposed repository contract, ownership and dependency graph, required checks/artifacts, publication overlay, placeholder removal, submodule state, and asset provenance rules.

Results:

- Added [PublicExampleRepositories.md](PublicExampleRepositories.md), ADR 0005, `Examples/PublicRepositories.json`, and a reusable `Examples/PublicRepositoryTemplate/` governance overlay. The registry defines four sequenced repositories, one source-ready template, stable owners, exact release pins, a weekly current-Engine lane, reviewed tags, required evidence, and exit gates.
- Added `BuildTools/docs_examples.py`, five focused tests, a deterministic machine model, generated Markdown reference, and repository verification in `pinned` and `current` modes. Standalone validation and GitHub Actions now enforce registry semantics, overlay completeness, generated freshness, workflow markers, exact gitlink/checkout agreement for releases, placeholder removal, and asset source/license/hash/path evidence.
- Made `Examples/MinimalProject` directly configurable through Windows and Linux presets. The Windows preset intentionally lets CMake select the newest installed Visual Studio instead of rejecting compatible newer installations with a hard-coded VS 2022 generator.
- Routed the human guide, generated reference, machine model, and ownership decision through the documentation manifest, site navigation/search, AI delivery, maintainer indexes, maintenance guidance, production plan, backlog, and Last Frontier Engine-update workflow.
- Reconciled updater protocol generation 2 and client host/runtime ABI 3 into the release policy. Repositories must publish a full client package across this frozen-host boundary; the current-Engine lane must not overwrite the exact release pin or claim release compatibility.

Mechanical checks:

- Focused public-example tests: 5 passed. Complete `test_docs*.py` discovery: 117 passed. `docs_validate.py` validates 95 maintained Markdown entries.
- All 13 API/CMake/CLI/helper/native-extension/prototype/map/package/reference/inventory/public-example/site/AI generated-document checks are current. The public-example model contains four repositories, one source-ready repository, and zero published repositories.
- The site model contains 62 navigation items and 87 searchable documents in 652,423 bytes. AI delivery contains 89 public documents and 894,249 full-context bytes.
- Three structural CMake interface tests passed. The preserved-baseline aggregate diff remains at four changes across eight domains, with two required dispositions satisfied and zero missing.
- `win64-starter-smoke` on Engine `1bcf6e101` reached native-extension value `42`, `starter_server_started`, `starter_smoke_passed`, and clean shutdown with updater protocol generation 2. The standalone `windows` preset then configured cleanly with CMake-selected `Visual Studio 18 2026`, MSVC 19.51, and the same Engine revision. The final Engine advance to `2f4fc0adf` is test-only and does not invalidate those runtime/configuration results.
- Final `RunUnitTests` on Engine `2f4fc0adf` passed 341 test cases and 355,926 assertions. Last Frontier `BakeResources` then completed for project version `0.3.532`, compiling scripts and baking all 612 maps; `factions.guard_report_offense_restores_exact_caller_cover` passed 1/1 with zero failures, timeouts, skips, or global exception delta on the rebuilt updater-generation-2 `LF_ServerHeadless`.
- Final fetches report Last Frontier `ac841fd791cb371d23e93df83e598ebbb5bdc27e` and Engine `2f4fc0adfdabf71316f087bf36ceb6baf49c81da` at `0 0` against upstream. The root gitlink matches Engine HEAD, staged and unmerged sets are empty, and `git diff --check` passes. Four task-created safety stashes were removed by verified hash; the two older root and two older Engine stashes remain untouched.

Follow-up:

- Owner authorization is still required before creating or publishing `cvet/fonline-project-template`; materialize it from `Examples/MinimalProject` plus the checked overlay, replace every placeholder, pin the exact Engine gitlink, enable repository security/branch protection, pass both compatibility lanes, and publish the first reviewed tag.
- Keep the other three repositories blocked on their recorded dependencies and exit gates. Confirm the first landed public-example, documentation-site, AI-delivery, and `jekyll-build-pages` artifacts in GitHub Actions before treating external publication as complete.

## 2026-07-16 - model animation metadata and duration reference

Scope:

- Safe Last Frontier update from `ac841fd791cb371d23e93df83e598ebbb5bdc27e` to `aa0d3b5e0a31ec92fa2efb9196282d66ad348025` and Engine update from `2f4fc0adfdabf71316f087bf36ceb6baf49c81da` to `fe7fb1e73af4d66a5ddd37828b5dff1544d54884`, with complete incoming-range audits and named `include-untracked` safety stashes around both dirty worktrees.
- Reusable `.fo3d` animation tuple metadata, speed scaling, state/action aliases, effective duration, private baked metadata, common script lookup, client loaded-model distinction, diagnostics, and embedding-project boundaries.
- Human, AI, site, generated-reference, maintenance, backlog, production-plan, and Last Frontier routing for the new guide.

Source areas checked:

- `Source/Tools/ModelInfoBaker.cpp`, model-info parsing and registration, common and client script bindings, loaded-model animation lookup/substitution, metadata access, focused native model-baker tests, and current generated script API.
- The complete incoming root and Engine ranges. The final root range changes combat speech, AI/fleeing behavior, synchronization helpers/audits, GUI, focused tests, and matching project documentation. The final Engine range only advances `MigrationRule Version` to `0 0 31`; generated compatibility and API artifacts were refreshed.
- Existing baking, generated-metadata, tool, method-map, documentation-maintenance, site/AI-delivery, contract-diff, and project-integration guidance.

Results:

- Added [ModelAnimation.md](ModelAnimation.md), defining `Anim`, `AnimSpeed`, `StateAnimEqual`, and `ActionAnimEqual` from current source, including one-step alias behavior, source-tuple priority, unresolved-entry omission, and effective duration as `round((clip duration / speed) * 1000)` milliseconds.
- Documented `ModelAnimationInfo.foinfo` as private baker/runtime metadata rather than an authored contract. Common `Game.GetModelAnimDuration` returns zero for missing tuples; client `Critter.GetModelAnimDuration` queries the loaded model and may follow cross-model substitutions.
- Added five source-backed documentation tests and made them mandatory in standalone validation and GitHub Actions. Routed the guide through the manifest, navigation/search, AI artifacts, indexes, maintenance triggers, and Last Frontier content/build references.
- Regenerated the API and reference inventory at 2,461 entries, including 948 methods. The site contains 63 navigation items and 88 searchable documents in 658,355 bytes; AI delivery contains 90 public documents and 907,903 full-context bytes.

Mechanical checks:

- Focused model-animation documentation tests: 5 passed. Standalone validator tests: 29 passed. Complete `test_docs*.py` discovery: 123 passed; `docs_validate.py` validates 96 maintained Markdown entries.
- All 13 API/CMake/CLI/helper/native-extension/prototype/map/package/reference/inventory/public-example/site/AI generated-document checks are current. Three structural CMake interface tests passed.
- Preserved-baseline aggregate diff: 5 changes across 8 domains, 2 required dispositions satisfied, and 0 missing dispositions. The API delta includes the migration-version advance and additive internal `Game.GetModelAnimDuration` method.
- Focused native model-animation tests passed 3 test cases and 170 assertions. The authoritative `RunUnitTests` target passed 342 test cases and 356,125 assertions. An earlier randomized invocation exited without a Catch2 failure summary; focused and direct full reruns were green before the authoritative target rerun.
- Last Frontier `BakeResources` completed a clean full rebuild for project version `0.3.534`; scripts, 64 model-info files, prototypes, texts, and all 612 maps baked without warnings or errors. The previously recorded `footsteps` duration/cadence regression passed 8/8. After final synchronization, `combat_speech` passed 9/9 and the complete filtered run passed 10/10 with zero failures, timeouts, skips, or global exception delta.
- Incoming synchronization-audit tests passed 71/71. The final root and Engine parity, gitlink, staging, unmerged, diff, and stash checks are recorded in the active Last Frontier plan after the closing fetch.

Follow-up:

- Complete the broader source-backed model contract: full `.fo3d` composition, model layers, mesh/material/texture behavior, root motion, rendering/runtime substitution, and asset-pipeline validation.
- Confirm the landed documentation, contract-diff, site, AI-delivery, and `jekyll-build-pages` artifacts in GitHub Actions. Production Pages source confirmation and public example-repository creation remain owner-gated.

## 2026-07-16 - 2D sprite root motion and walk-cycle reference

Scope:

- Continued from Last Frontier `aa0d3b5e0a31ec92fa2efb9196282d66ad348025` and Engine `fe7fb1e73af4d66a5ddd37828b5dff1544d54884`. Opening and closing fetches both reported upstream parity, so no incoming range or task safety stash was required.
- Reusable 2D sprite root motion from authored `NextX` / `NextY` frame offsets through baking, sprite-sheet loading, movement-phase selection, rendered offset, direction changes, lifecycle resets, and zero-vector fallback.
- Human, AI, site, maintenance, backlog, production-plan, and Last Frontier integration routing. Authoritative movement, networking, 3D skeletal animation, and the complete `.fo3d` grammar remain outside this slice.

Source areas checked:

- `ImageBaker::FrameShot`, FOFRM parsing, baked collection serialization, `DefaultSpriteFactory`, `SpriteSheet`, `MovingContext`, and the 2D walk/run path in `CritterHexView`.
- TLA snapshot `b603d8fdbc2b2f89f233b2a1938686ead9d8d480`, pinned to Engine `801da0753b3a4bc2d1f52d0a0297bc658006ec05`, was used for discovery only. Every promoted behavior was re-derived from the current Engine source.
- Existing movement, baking, client-runtime, tool, documentation-maintenance, site, and AI-delivery guidance plus the current native ImageBaker test surface.

Results:

- Added [SpriteRootMotion.md](SpriteRootMotion.md), separating 2D sprite root motion from `.fo3d` model animation and documenting source ownership, authoring, transport, activation, displacement, anchoring, phase projection, frame selection, rendered offset, transitions, fallback behavior, and embedding-project responsibilities.
- Added five source-backed documentation tests and made them mandatory in standalone validation and GitHub Actions. Routed the guide through the manifest, site navigation/search, AI artifacts, maintainer indexes, maintenance triggers, production planning, backlog, and Last Frontier content guidance.
- The guide states the ownership boundary explicitly: root motion selects and offsets rendered walk/run frames; it does not alter the logical path, speed, hex transitions, authoritative position, or network state.

Mechanical checks:

- Focused sprite-root-motion documentation tests: 5 passed. Standalone validator tests: 30 passed. Complete `test_docs*.py` discovery: 129 passed; `docs_validate.py` validates 97 maintained Markdown entries.
- All 13 API/CMake/CLI/helper/native-extension/prototype/map/package/reference/inventory/public-example/site/AI generated-document checks were current before this report entry; site and AI artifacts are regenerated again after recording it.
- Focused native ImageBaker validation passed 1 test case and 745 assertions, including expected malformed-input branches. Runtime/native behavior and project content did not change, so full `RunUnitTests` and Last Frontier `BakeResources` were not rerun for this documentation-only slice.
- The native suite has no focused `CritterHexView` root-motion fixture. A visible client scene remains the semantic validation gate for walk-cycle phase, direction changes, and perceived foot sliding.

Follow-up:

- Complete the independent `.fo3d` model-description and composition reference without conflating skeletal model animation with 2D sprite root motion.
- Consider adding a focused native `CritterHexView` phase/transition fixture and a reusable visible locomotion validation scene before changing the algorithm.
- Confirm the landed documentation, site, AI-delivery, and `jekyll-build-pages` artifacts in GitHub Actions; production Pages source confirmation remains owner-gated.

## 2026-07-16 - model-description format and reconnect reconciliation

Scope:

- Safe Last Frontier updates from `aa0d3b5e0a31ec92fa2efb9196282d66ad348025` through `513f9531c`, `95f0fb857`, `6811bb561`, and `8693be022`; Engine updates from `fe7fb1e73af4d66a5ddd37828b5dff1544d54884` through `5ce19ec24` and `dc630f17e`. Named root and Engine `include-untracked` safety stashes protected the dirty documentation program across every update.
- Independent reusable `.fo3d` documentation covering source meshes, parser grammar/state, layers, attachments, particles, transforms, textures/effects, cuts, animation integration, baking, runtime composition, diagnostics, and embedding-project validation.
- Reconciliation of the Engine caller-owned existing-player reconnect synchronization contract with Last Frontier authentication, plus every project behavior added by the incoming ranges.

Source areas checked:

- `ModelDescriptionParser`, `ModelInfoBaker`, `ModelMeshBaker`, `3dStuff`, render-time model limits, model-baker tests, generated project settings, and current TLA `.fo3d` evidence. TLA remained discovery evidence only; obsolete tokens and asset formats were not promoted.
- Engine login/chosen-critter lifecycle and synchronization changes, project authentication dispatch, local-map and global-map group ownership, generated API/migration data, exception-safety classifications, and the complete incoming root/Engine ranges.
- Incoming project faction AI, embedded-client PDA reload, Battalion identity/uniform/cursor behavior, fox pack behavior, analytics hard-disconnect cleanup, Antenna guard engagement radii, and AiControl client network statistics. Project-owned behavior stayed in Last Frontier docs and tests.

Results:

- Added [ModelFormat.md](ModelFormat.md), `BuildTools/ModelFormatInterface.json`, `BuildTools/docs_model_format.py`, a canonical generated JSON model, seven generated reference pages, and seven source-backed documentation tests.
- Added `model-format` as the ninth aggregate contract-diff domain and routed it through CI, the documentation manifest, navigation/search, AI delivery, generated-reference maintenance, public API notes, production planning, backlog, and Last Frontier authoring routes.
- Last Frontier now prepares the full existing-login synchronization cover before calling Engine: incoming/live players, the controlled critter, local map/location, or every stable global-map group member. The auth chain is explicitly async, the reusable ownership rule is documented in [ScriptLifecycleAndConcurrency.md](ScriptLifecycleAndConcurrency.md), and focused local/global regressions guard the integration.
- The first full native run exposed compressed test transport being parsed as raw messages. The lifecycle fixture now uses persistent stream decompression before inspecting message frames; final upstream `dc630f17e` adopted the same implementation, and the remaining local change is the explicit direct `Compressor.h` dependency.
- The project exception-safety audit writer now omits trailing empty TSV fields while preserving round trips, supports canonical `update --normalize`, and keeps the two changed server methods classified as `Basic`. The final audit checks 5,262 functions with zero errors or warnings.
- The closing project update keeps Antenna's dense headhunter population on tactical local aggro radii and exposes client ping/FPS through the AiControl observation and MCP schema. Its static-location and MCP tests are project-owned and do not introduce a reusable Engine documentation domain.

Mechanical checks:

- Focused model-format documentation tests: 7 passed. Complete `test_docs*.py` discovery: 137 passed; `docs_validate.py` validates 105 maintained Markdown entries.
- All API, CMake, main CLI, helper CLI, native-extension, prototype-format, map-format, model-format, package, reference, inventory, public-example, site, and AI-delivery generated outputs are current. The site model contains 66 navigation items and 97 searchable documents in 699,781 bytes; AI delivery contains 99 public documents and 958,741 full-context bytes. Three structural CMake interface tests pass.
- Preserved-baseline aggregate contract diff: 2 changes across 9 domains, with 0 missing dispositions. The changes are compatibility migration `0.0.32` and the documented internal `Game.LoginPlayerToExistentRecord` synchronization contract.
- Focused native ModelBaker validation passed 1 test case and 16 assertions. Final authoritative `RunUnitTests` on Engine `dc630f17e` passed 342 test cases and 356,156 assertions.
- Last Frontier `BakeResources` completed clean full rebuilds for project version `0.3.535`, compiling scripts and baking 64 model-info files and all 612 maps. The rebuilt `LF_ServerHeadless` passed 9/9 focused reconnect, faction AI, fox-pack, analytics logout, PDA reload, Battalion cursor/identity, and Antenna guard-radius tests with zero failures, timeouts, skips, or global exception delta.
- The complete AiControl MCP suite passed 1,522 tests. Its first post-update run exposed a stale hard-coded authored-exit coordinate; the path-selection regression now derives the current first target from the parsed map contract while still proving that an unreachable group is skipped for the reachable second group.
- Final fetches report Last Frontier `8693be022b9a7f6d3df4227d5cd2cee4d6afea5a` and Engine `dc630f17e1281358bbf2b603ca4bfd257cc27c94` at `0 0` against upstream. The root gitlink matches Engine HEAD, staged and unmerged sets are empty, and `git diff --check` passes. Six task-created safety stashes were removed by verified hash; the two older root and two older Engine stashes remain untouched.

Follow-up:

- Publish the first owner-approved repository from [PublicExampleRepositories.md](PublicExampleRepositories.md), then use its review feedback to refine the reusable model example without weakening exact Engine pinning or provenance gates.
- Confirm the landed nine-domain contract-diff, documentation-site, AI-delivery, `RunUnitTests`, and `jekyll-build-pages` artifacts in GitHub Actions. Production Pages source confirmation remains administrator work.

## 2026-07-16 - documentation version, locale, and stable route contract

Scope:

- Continued from Last Frontier `8693be022b9a7f6d3df4227d5cd2cee4d6afea5a` and Engine `dc630f17e1281358bbf2b603ca4bfd257cc27c94`. Opening fetches reported upstream parity in both repositories, so no incoming range or task safety stash was required.
- Converted the accepted rolling-version, bilingual layout, stable URL, and redirect decisions into one machine-readable and CI-enforced Engine contract without moving the English corpus or beginning Russian translation.
- Kept GitHub Pages/Jekyll and repository Markdown as the only publication architecture.

Source areas checked:

- `Docs/documentation-manifest.json`, ADRs 0001 through 0005, `Docs/SitePublication.md`, the production plan, backlog, maintenance workflow, and human/AI entry points.
- `BuildTools/docs_ai_delivery.py`, `BuildTools/docs_site.py`, `BuildTools/docs_validate.py`, their focused tests, the default Jekyll layout, generated site/search/AI outputs, and the GitHub Pages workflow.
- Last Frontier Engine-update maintenance guidance and the active project plan, so future root/Engine updates regenerate site, route, and AI data in dependency order.

Results:

- Added [ADR-0006](Decisions/0006-documentation-version-locale-routing.md). The unversioned site is now explicitly the rolling `current` channel on `master`; tagged snapshots remain deferred until supported release lines and a support matrix exist.
- Added source-owned `versioning` and `localization` sections to the documentation manifest. English remains canonical, Russian remains planned, `Docs/en` targets mirror to `Docs/ru`, and five README-style entry points have explicit locale pairs.
- Extended `BuildTools/docs_site.py` to schema 2 and generated [document-routes.json](generated/document-routes.json). The model records current URLs, canonical future owners, planned English/Russian paths, availability, and every legacy route that must survive a move.
- Multiple old pages may converge only when exactly one non-`replace` document owns the future target. The current public API routes correctly converge on the generated API index.
- Added a validated `redirect` document state with `> Legacy route.` marker, non-human/non-search classification, stable target ID, shared canonical target, a direct Markdown link to that canonical file, and generated redirect ownership. This lets old Markdown URLs remain readable in GitHub and Jekyll after a move without generated HTML or another redirect plugin.
- Site navigation data and public `docs-manifest.json` now expose the same rolling version and locale policy. The layout labels `Current master` from generated data instead of maintaining an independent version string.
- Updated publication, generated-metadata, maintenance, backlog, production-plan, BuildTools, human/AI index, and Last Frontier Engine-update guidance. Physical `Docs/en` / `Docs/ru` migration, language switching, translation hashes/parity, and reviewed translations remain pending.

Mechanical checks:

- Focused AI-delivery tests passed 7/7, site generator tests 8/8, layout tests 5/5, and standalone-validator tests 35/35.
- Complete `test_docs*.py` discovery passed 146 tests. `docs_validate.py` validates 106 maintained Markdown entries.
- The generated site contains 67 navigation items and 98 searchable documents in 704,751 bytes. The route catalog contains 100 public routes, 94 planned legacy redirects, 97 canonical translation targets, and zero completed translation pairs, so no missing Russian page is presented as current.
- AI delivery contains 100 public document records; `llms-full.txt` is 972,340 bytes against the 1,048,576-byte hard limit. Its public manifest reports `current/master`, deferred release snapshots, canonical `en`, and the route-catalog artifact hash.
- Python compilation, generated `--check` gates, standalone link/source/manifest validation, and `git diff --check` pass. This slice changes documentation tooling and prose only, so native unit tests and Last Frontier resource baking were not rerun.
- Ruby and Bundler are not installed on this host. GitHub Actions `jekyll-build-pages` remains the authoritative Liquid render and publication gate.
- Closing fetches report Last Frontier `8693be022b9a7f6d3df4227d5cd2cee4d6afea5a` and Engine `dc630f17e1281358bbf2b603ca4bfd257cc27c94` at `0 0` against upstream. The root gitlink matches Engine HEAD, no task stash was created, and the four pre-existing user stashes remain untouched.

Follow-up:

- Keep full Russian translation blocked until the first execution slice has a green Linux starter run and the remaining English coverage is ready for the translation-pass freeze.
- Migrate public pages in reviewed groups: create the canonical `Docs/en` page, retain the old Markdown route as a validated pointer, add the reviewed `Docs/ru` counterpart, then enable language-preserving navigation and translation-hash parity.
- Confirm the landed documentation-site, route-catalog, AI-delivery, and `jekyll-build-pages` artifacts on `fonline.ru`; Pages source branch/folder confirmation remains administrator work.

## 2026-07-16 - text and localization format reference

Scope:

- Safe Last Frontier update from `8693be022b9a7f6d3df4227d5cd2cee4d6afea5a` to `2dd4968c03b765a29cc09c3ecf722102c45488c2` and Engine update from `dc630f17e1281358bbf2b603ca4bfd257cc27c94` to `dc124039423df71931cf3d7fd18a9664b20a469c`, with named root/Engine safety stashes retained through reconciliation and validation.
- Independent reusable documentation for raw `.fotxt`, structured keys and variants, ordered language baking/fallback, prototype `$Text`, runtime script lookup, language switching, renderer color tags, diagnostics, authoring practices, and the embedding-project formatting boundary.
- Last Frontier integration cleanup for its Russian-first policy, concrete packs, semantic key conventions, translation guards, `TextFormatting.fos`, and GUI refresh behavior. TLA revision `b603d8fdbc2b2f89f233b2a1938686ead9d8d480` remained historical integration evidence only.

Source areas checked:

- `Source/Common/TextPack.*`, `Source/Tools/TextBaker.*`, `Source/Tools/ProtoTextBaker.*`, `Source/Common/Settings.inc`, client/server language loading, common/client/server script methods, AngelScript text value types, font inline-color parsing, and focused native text tests.
- Last Frontier `Docs/Localization.md`, `Scripts/TextFormatting.fos`, configured packs/languages, translation helpers/guards, and current project lookup conventions.
- TLA `README.md`, `Texts/`, and `Texts/Game.engl.fotxt` for historical numeric keys, duplicate variants, inline color, and project lexem evidence; no project-only formatter was promoted into Engine.

Results:

- Added [TextAndLocalization.md](TextAndLocalization.md), `BuildTools/TextFormatInterface.json`, `BuildTools/docs_text_format.py`, [generated/text-format.json](generated/text-format.json), six generated reference pages, and seven source-backed documentation tests.
- The generated model contains 38 stable entries: 7 syntax rules, 9 language rules, 8 prototype-text rules, 7 runtime methods, 2 rendering rules, and 5 validation rules. Engine defaults and the five prototype output packs are derived from live source.
- Added `text-format` as the tenth aggregate contract-diff domain and routed it through CI, standalone validation, the documentation manifest, navigation/search, AI delivery, generated-reference maintenance, public API notes, production planning, backlog, and Last Frontier integration routes.
- Corrected two project documentation errors: four-character language identifiers are a Last Frontier convention rather than an Engine parser requirement, and script `Game.GetText(key, skipCount = 0)` selects the first indexed variant rather than choosing a duplicate randomly.
- That source correction exposed a Last Frontier integration gap: `CombatSpeech::ReceiveCombatSpeech` currently uses the no-index overload, so its authored multi-line pools always resolve variant `0`. Project localization/combat docs now state the live behavior rather than promising random rotation.
- Separated Engine renderer-owned `@color` push/reset tags from Last Frontier-owned `@pname@`, `@arg@`, `@text@`, `@rnd@`, gender, and variant formatting.
- The preserved-baseline aggregate run exposed false map-format breaks caused only by derived `enum_source.line` movement. The comparator now excludes enum source provenance from contract digests/diffs, with a regression test proving line movement produces zero map-format changes.

Mechanical checks:

- Focused text-format tests: 7 passed; aggregate contract-diff tests: 9 passed; standalone-validator tests: 36 passed.
- Complete `test_docs*.py` discovery: 155 passed. `docs_validate.py` validates 113 maintained Markdown entries.
- All API, reference, map-format, text-format, site, AI-delivery, and remaining generated checks are current. The site contains 69 navigation items and 105 searchable documents in 727,071 bytes; the route catalog contains 107 public routes and 101 planned redirects.
- AI delivery contains 107 public document records; `llms-full.txt` is 992,872 bytes against the 1,048,576-byte limit.
- Preserved-baseline aggregate contract diff: 2 internal/API changes across 10 domains, with 0 required and 0 missing dispositions. The new text-format domain has zero baseline-to-current drift after bootstrap-equivalent seeding.
- This slice changes documentation tooling and prose only. Native text behavior, Last Frontier content, and runtime code did not change, so native unit tests and `BakeResources` were not rerun.
- Closing fetches report Last Frontier `2dd4968c03b765a29cc09c3ecf722102c45488c2` and Engine `dc124039423df71931cf3d7fd18a9664b20a469c` at `0 0` against upstream. The root gitlink matches Engine HEAD, staged and unmerged sets are empty, both `git diff --check` runs pass, the two task safety stashes were removed by verified hash, and the two older root plus two older Engine stashes remain untouched.

Follow-up:

- Add a public minimal localization example after the first template repository is owner-approved, including one raw pack, one prototype `$Text` entry, explicit variant selection, and a visible language-switch check.
- Restore Last Frontier combat-speech rotation as a gameplay change with explicit index selection and a deliberate server/client ownership decision; extend the focused suite beyond pool richness to prove runtime selection.
- Continue the remaining authored-format program with images, particles, GUI, and dialogs from their owning parsers/bakers rather than project prose.
- Confirm the landed eleven-domain diff, documentation-site, AI-delivery, and `jekyll-build-pages` artifacts in GitHub Actions; production Pages source confirmation remains administrator work.

## 2026-07-16 - effect format reference and update reconciliation

Scope:

- Safe Last Frontier update from `2dd4968c03b765a29cc09c3ecf722102c45488c2` to `5c500fd3bdc42e88720380b0d814999e470a6408` and Engine update from `dc124039423df71931cf3d7fd18a9664b20a469c` to `204d223d6ef6514494c4af053c323aa2c73e0a48`, with named root/Engine safety stashes retained through reconciliation and validation.
- Independent reusable documentation for `.fofx` sections, pass-specific shader/state fallback, render state, vertex/resource contracts, backend artifacts, runtime loading and path-only caching, script methods, `ScriptValueBuf` lifetime, diagnostics, and embedding-project responsibilities.
- Reconciliation of the incoming hard-error `InitScript` resolution contract, Last Frontier documentation ownership, and the project sync-flow CI move.

Source areas checked:

- `EffectBaker`, `RenderEffect`, all active renderer backends, `EffectManager`, client effect script methods, CMake effect limits, built-in effect sources, and focused native effect-baker tests.
- Last Frontier `Resources/Visual/Effects/`, effect selection/tuning scripts, project render settings, content validation routes, and existing shader policy.
- The complete incoming root and Engine ranges, including `ScriptHelpers::CallInitScript`, entity initialization, prototype callback metadata, exception-safety classifications, and sync-flow workflow ownership.

Results:

- Added [EffectFormat.md](EffectFormat.md), `BuildTools/EffectFormatInterface.json`, `BuildTools/docs_effect_format.py`, a canonical generated JSON model, seven generated reference pages, and seven source-backed documentation tests.
- Added `effect-format` as the eleventh aggregate contract-diff domain and routed it through CI, the standalone validator, manifest, navigation/search, AI delivery, public API notes, maintenance, production planning, backlog, and Last Frontier authoring routes.
- The generated effect model contains 55 contract entries, 12 built-in resource records, and 8 validation rules. It records that `SetEffect` selects a path-cached `RenderEffect`; it does not clear cached script values or transfer them between paths. Last Frontier comments and docs now explain why tuning tools still re-push panel state after a variant change.
- The incoming built-in `InitScript` callback contract is now documented in the prototype and script-lifecycle references. Its intentional prototype-format scope expansion has an exact shared-ledger disposition with migration guidance for missing or mismatched `void(Entity, bool firstTime)` callbacks.
- The standalone validator fixture now isolates text/effect source anchors without one generated domain overwriting another, excludes generated effect detail pages from authored inventory checks, and the aggregate comparator no longer reports the obsolete "all four models" diagnostic.

Mechanical checks:

- Focused effect-format documentation tests passed 7/7. Complete `test_docs*.py` discovery passed 164 tests; `docs_validate.py` validates 121 maintained Markdown entries.
- Effect, site, AI-delivery, and aggregate contract generated checks pass. The site model contains 71 navigation items, 113 searchable documents, 115 public routes, and 109 planned redirects; its search artifact is 762,325 bytes.
- AI delivery contains 115 public documents; `llms-full.txt` is 1,029,603 bytes against the 1,048,576-byte hard limit.
- Preserved-baseline aggregate contract diff passes with 4 changes across 11 domains, 1 required disposition, and 0 missing dispositions. The reported changes are the current compatibility migration, the `LoginPlayerToExistentRecord` documentation update, and the documented `InitScript` scope/rule additions.
- The rebuilt `LF_UnitTests` binary passes all 4 focused `EffectBaker*` test cases with 106 assertions. Last Frontier `BakeResources` completed a clean full rebuild for version `0.3.539`, including scripts, 64 model-info files, and all 612 maps.
- The project sync-flow audit passes 111 tests. The reconciled exception-safety audit checks 5,262 functions with zero errors or warnings.
- Closing fetches report Last Frontier `5c500fd3bdc42e88720380b0d814999e470a6408` and Engine `204d223d6ef6514494c4af053c323aa2c73e0a48` at `0 0` against upstream, with the root gitlink matching Engine HEAD.

Follow-up:

- Continue the authored-format program with image/sprite source formats beyond root motion, particles, GUI, and dialogs from their owning parsers, bakers, and reusable modules.
- Add public example effects only after the first repository is owner-approved; keep every sample pinned to an exact Engine revision and validate the intended renderer/backend profile visibly.
- Confirm the landed eleven-domain contract-diff, documentation-site, AI-delivery, focused native, and `jekyll-build-pages` artifacts in GitHub Actions; production Pages source confirmation remains administrator work.

## 2026-07-18 - image and sprite format reference and update reconciliation

Scope:

- Safe Last Frontier update from `5c500fd3bdc42e88720380b0d814999e470a6408` to `819e45ec2b0df61c4bc172560cf7894a74357cda` and Engine update from `204d223d6ef6514494c4af053c323aa2c73e0a48` to `65dfb851c7b111e9e613be98c6bc1d49d3d61145`, with twelve named root/Engine `include-untracked` safety stashes retained through opening, closing, final, late third-party, closure, and upstream-fix reconciliation, then removed by verified hash after all gates passed. The root gitlink remains at `f667a85d9`; the deliberate two-commit working delta is the audited upstream rpmalloc/MSVC plus glslang warning fix.
- Independent reusable documentation for accepted image sources, FOFRM composition, per-file options, frame metadata, baked sprite collections, runtime factories, atlases, caches, diagnostics, validation, and embedding-project responsibilities.
- Reconciliation of all 24 incoming Last Frontier commits and eleven incoming Engine commits. Project-owned gameplay, synchronization, authentication, analytics, AI, quest, map-authoring, test-runner, and project dependency behavior remain in Last Frontier docs; reusable runtime and dependency behavior remain in their Engine owners.

Source areas checked:

- `ImageBaker`, all registered raster and legacy decoders, FOFRM parsing and flattening, baked collection serialization, `DefaultSpriteFactory`, `SpriteSheet`, `TextureAtlas`, `ResourceManager`, image settings, and focused native tests.
- Current Last Frontier PNG, TGA, and FOFRM usage for integration evidence. Project asset catalogs, art direction, pack policy, and visual composition remain project-owned; legacy binary formats are documented as import paths rather than recommended authoring formats.
- The complete incoming root and Engine ranges, including server entity-lock ownership, multi-target movement, `ItemView` handle comparison, malformed compressed-stream handling, personal-room fallback, crafting cadence, quest migration, tunnel-exit coverage, the network-client header guard, accepted-connection ownership, pre-login progress deadlines, engine dependency/toolchain refreshes, project-local curl/sentry refreshes, and their reusable/project documentation owners.

Results:

- Added [ImageFormat.md](ImageFormat.md), `BuildTools/ImageFormatInterface.json`, `BuildTools/docs_image_format.py`, [generated/image-format.json](generated/image-format.json), seven generated reference pages, and seven source-backed documentation tests.
- The generated model contains 49 entries. It derives 12 baker extensions (`fofrm`, `frm`, `fr0`, `rix`, `art`, `spr`, `zar`, `til`, `mos`, `bam`, `png`, `tga`) and 11 stock runtime extensions from current source. Direct `.spr` is intentionally identified as baker-only in the stock runtime route; projects should wrap it through FOFRM unless they extend the factory.
- Documented FOFRM aliases, relative `$` references, direction completeness, Main-sequence flattening, inherited offsets, descriptor-count timing, shared-record rejection, and the current parsed-but-unserialized `EffectName` behavior. The guide also distinguishes the stable baked RGBA8 collection from authored syntax and runtime atlas/cache policy.
- Added `image-format` as the twelfth aggregate contract-diff domain and routed it through CI, standalone validation, the source manifest, navigation/search, AI delivery, generated-reference maintenance, public API notes, production planning, backlog, and Last Frontier authoring routes.
- The complete image owner raised `llms-full.txt` above the old 1 MiB budget. ADR 0003 and the source manifest now set a reviewed 1.25 MiB (1,310,720-byte) fail-closed limit; generation still rejects overflow and never truncates a document. The separate search-index limit remains 1 MiB.
- Rebuilding current unit tests initially exposed an incoming test-only include cycle: `Test_NetworkClient.cpp` included both `ClientConnection.h` and its then-unguarded transitive `NetworkClient.h`. The final Engine range restores the owning header guard; the local redundant-include cleanup remains behavior-neutral, and the target builds on the final Engine revision.
- Root reconciliation corrected project documentation tails left by the incoming code: `DefaultPersonalRoomLocation` covers any null/non-room respawn, `Mobs::SpawnMob` has a monotonic caller-cover postcondition, caravan materialization remains independently cover-neutral, every visible `ToGlobalArea_*` tunnel exit requires hidden `ExitGrid.MultihexMesh` coverage, and project auth policy routes reusable accepted-connection and pre-login timeout mechanics to [Networking.md](Networking.md).
- The late dependency refresh is owned by [ThirdPartyMaintenance.md](ThirdPartyMaintenance.md), `ThirdParty/README.md`, and the incoming Last Frontier refresh plan. The project reconciliation also updates stale operational version extraction to libcurl 8.21.0 and sentry-native 0.15.3 and makes their source headers the documented version authority; no image-format contract changed.
- Native validation exposed two integration gaps in the refreshed Engine pins on MSVC. The owning upstream commits add a reusable `TargetCompileOptions` wrapper, build rpmalloc 2.x as C11 with `/experimental:c11atomics`, and rename glslang's shadowing local in the vendored header with an explicit `(FOnline Patch)` marker. This checkout also makes the declared C standard required. Clean `BakerLib`, `LF_UnitTests`, `LF_ServerHeadless`, and full project bake rebuilds pass without the incoming `C4458` warning.
- The closure root commit separates production sync-flow collection from the full gameplay-test graph, removes obsolete baseline entries, strengthens Irvin and tunnel survival/lock tests, and teaches the Windows updater test to skip Direct3D only when Session 0 cannot create a swap chain. Project testing and multithreading docs now describe the live two-gate CI topology and conditional renderer coverage.
- Sync-flow analysis found four stale auth-wrapper `SyncScope` promises, four missing Irvin quest lifecycle reproofs in production code, one missing tunnel-location cover, and the corresponding Irvin lifecycle boundaries in the registered gameplay test. The source now proves those contracts explicitly; six stale production entries and the complete touched Irvin callback owner were removed from their baselines instead of accepting new debt.

Mechanical checks:

- Focused image-format documentation tests passed 7/7, aggregate contract-diff tests 9/9, AI-delivery tests 8/8, and standalone-validator tests 37/37. Complete `test_docs*.py` discovery passed 172 tests; `docs_validate.py` validates 129 maintained Markdown entries.
- Image, API, reference, inventory, site, and AI artifacts were regenerated in dependency order and pass their freshness checks. The API model contains 950 methods, 133 properties, 121 events, and 266 settings. The site model contains 73 navigation items, 121 searchable documents, 123 public routes, and 117 planned redirects; its search artifact is 798,919 bytes against the 1 MiB limit.
- AI delivery contains 123 public documents; `llms-full.txt` is 1,069,024 bytes against the reviewed 1.25 MiB (1,310,720-byte) limit. Aggregate comparison against the opening Engine revision reports bootstrap status across all 12 domains with zero required or missing dispositions.
- The rebuilt final-revision `LF_UnitTests` target passes all 346 test cases and 356,278 assertions. This includes `ImageBaker` with 745 assertions, `TextureAtlasSpaceNode` with 40, seven `NetworkServer*` tests with 47, and `ServerDisconnectsPreLoginConnectionAfterLoginTimeout` with 39; earlier incoming compressor and malformed-client-input regressions pass another 12 and 6 assertions.
- Production sync-flow checks analyze 6,925 functions after excluding gameplay-test scripts during collection and accept all 1,685 current diagnostics against a reduced 1,388-entry baseline with zero new or stale entries. The isolated testing-callback gate still collects the complete 10,367-function graph and accepts all 4,881 current diagnostics against its reduced 3,395-entry baseline, also with zero new or stale entries; all 113 analyzer tests pass.
- The complete MCP adapter suite passes 1,633/1,633 tests. The reconciled exception-safety audit checks 5,274 functions with zero errors or warnings after a structured three-way baseline merge retained current upstream inventory plus reviewed local classifications.
- Last Frontier `ForceBakeResources` completed full rebuilds during dependency reconciliation for version `0.3.554` and again on final Engine `65dfb851c` for version `0.3.555`. Both include 11,864 CommonArt files, 1,429 InterfaceArt files, 818 CrittersArt files, scripts, prototypes, texts, 64 model-info files, and all 612 maps. The focused quest, encounter, dialog, and tunable-setting run passed 6/6, both reconnect/auth covers passed 2/2, and the final Engine-tip tunnel/Irvin rerun passed 2/2 with zero failures, timeouts, skips, or global exception delta.
- The project curl TLS source gate passes with peer verification enabled and strict host verification (`VERIFYPEER=1`, `VERIFYHOST=2`). The reconciled dependency-refresh plan now makes host MSVC an explicit complementary validation lane for future allocator and vendored-header updates.
- Closing fetches report Last Frontier `819e45ec2b0df61c4bc172560cf7894a74357cda` and Engine `65dfb851c7b111e9e613be98c6bc1d49d3d61145` at `0 0` against upstream. The root gitlink deliberately remains `f667a85d99394071e6edcb2501371e1c2f6b07c5`; staged and unmerged sets are empty, both `git diff --check` runs pass, all twelve task safety stashes were removed by verified hash, and the two older root plus two older Engine stashes remain untouched.

Follow-up:

- Continue the remaining authored-format program with particles, GUI, and dialogs from their owning parsers, bakers, runtime modules, and tests.
- Add a minimal public image/FOFRM example only after the first example repository is owner-approved; keep modern authored assets inspectable and legacy binary fixtures narrowly licensed and provenance-audited.
- Confirm the landed twelve-domain diff, documentation-site, AI-delivery, focused native, and `jekyll-build-pages` artifacts in GitHub Actions; production Pages source confirmation remains administrator work.

## 2026-07-19 - particle format reference and update reconciliation

Scope:

- Safe Last Frontier update from `819e45ec2b0df61c4bc172560cf7894a74357cda` to `d583aba4f59840133fa1d78f6aa563a0bbd8930c` and Engine update from `65dfb851c7b111e9e613be98c6bc1d49d3d61145` to `42b038cf5ac60f3533e8a760505da95078a900c5`, with named root and Engine `include-untracked` safety stashes retained through reconciliation and validation.
- Independent reusable documentation for `.fopts` SPARK XML, registered graph objects, the stock editor, raw-copy baking, client caching and cloning, renderer fields, atlas/direct-scene/model integration, diagnostics, performance, and embedding-project responsibilities.
- Complete reconciliation of the incoming Engine EffectBaker warning boundary and the incoming Last Frontier Battalion/Lendale quest, test, documentation, and playthrough changes. Neither incoming range changes the reusable particle contract.

Source areas checked:

- Vendored SPARK object registration, descriptors, XML loader/saver, graph validation, reference handling, and the exact Engine-owned `SparkQuadRenderer` registration.
- `VisualParticles`, `SparkExtension`, `ParticleSprites`, `ParticleEditor`, `RawCopyBaker`, model-info baking/runtime, script integration, settings defaults, focused native tests, and current Last Frontier authored particle systems and textures.
- The complete incoming root and Engine ranges. Project quest behavior remains in Last Frontier docs/tests; the warning-suppression scope remains in Engine build and third-party maintenance ownership.

Results:

- Added [ParticleFormat.md](ParticleFormat.md), `BuildTools/ParticleFormatInterface.json`, `BuildTools/docs_particle_format.py`, [generated/particle-format.json](generated/particle-format.json), eight generated reference pages, and eight source-backed documentation tests.
- The generated model contains 96 entries, 37 object registrations, and 12 renderer fields. It derives the 36 SPARK core registrations plus the Engine renderer, proves editor parity, and rejects object, renderer, setting, source-anchor, and generated-output drift.
- Added `particle-format` as the thirteenth aggregate contract-diff domain and routed it through CI, standalone validation, manifest, navigation/search, AI delivery, generated-reference maintenance, public API notes, production planning, backlog, and Last Frontier authoring routes.
- Documented that `.fopts` is copied unchanged rather than compiled, the XML graph must contain exactly one `System`, failed base-system loads are path-cached, instances are deep-copied, one `draw in scene` renderer promotes the whole sprite, model-bone particles use a separate runtime route, and `blend mode` is currently declared but unused.
- The audit corrected adjacent model documentation drift: `.fope` was replaced with the live `.fopts` suffix in the model contract, guide, and focused native fixture. The reusable `ModelProjFactor` default is now documented as `40`; Last Frontier retains its project override of `32`.
- Last Frontier routes reusable mechanics to the Engine guide while retaining its seven particle systems, six textures, selected effects, shot/grenade/model/scene integrations, render overrides, performance policy, and visible acceptance gates.

Mechanical checks:

- Focused particle documentation tests pass 8/8. Complete `test_docs*.py` discovery passes 180 tests; `docs_validate.py` validates 138 maintained Markdown entries.
- Particle, model, inventory, site, and AI generated checks are current. The site model contains 75 navigation items, 130 searchable documents, 132 public routes, and 126 planned redirects; its search artifact is 832,870 bytes against the 1 MiB limit.
- AI delivery contains 132 public documents; `llms-full.txt` is 1,099,989 bytes against the reviewed 1.25 MiB (1,310,720-byte) limit. Aggregate comparison against the opening Engine revision reports bootstrap status across all 13 domains with zero required or missing dispositions.
- The rebuilt `LF_UnitTests` target passes focused `RawCopyBaker` (15 assertions), `ModelBakers` (16 assertions), and four `EffectBaker*` cases (106 assertions), followed by a successful complete native suite.
- Last Frontier `BakeResources` completes a clean full rebuild for version `0.3.555`, including 11,864 CommonArt files, 1,429 InterfaceArt files, 818 CrittersArt files, scripts, 64 model-info files, and all 612 maps. The reconciled quest run passes 275/275 tests across 14 suites with zero failures, timeouts, skips, or global exception delta; the quest suite itself passes 177/177.
- The complete AiControl MCP suite passes 1,645/1,645 tests. A visible Direct3D `DevTest` smoke run with `SceneParticlesDemo.Enabled = True` renders all four cursor-wave groups and confirms scene-depth occlusion; the local capture is `Workspace/ParticleDocsLive/direct3d.png`, and no temporary capture code remains in authored source.
- Ruby and Bundler are unavailable on this host, so local Liquid rendering could not run. GitHub Actions `jekyll-build-pages` remains the authoritative site-render and publication gate.
- Closing fetches report Last Frontier `d583aba4f59840133fa1d78f6aa563a0bbd8930c` and Engine `42b038cf5ac60f3533e8a760505da95078a900c5` at `0 0` against upstream. The root gitlink matches Engine HEAD, staged and unmerged sets are empty, both `git diff --check` runs pass, and the two task safety stashes were removed by verified hash while the two older root and two older Engine stashes remain untouched.

Follow-up:

- Continue the remaining reusable format/tooling program with GUI, dialogs, fonts, audio/video, and focused editor manuals from their owning source and tests.
- Publish the first owner-approved repository from [PublicExampleRepositories.md](PublicExampleRepositories.md), then use its review feedback to refine the minimal particle example while preserving exact Engine pins and asset provenance.
- Confirm the landed thirteen-domain diff, documentation-site, AI-delivery, native/project checks, and `jekyll-build-pages` artifacts in GitHub Actions. Production Pages source confirmation remains administrator work.

## 2026-07-19 - font format and text layout reference and final update reconciliation

Scope:

- Safe Last Frontier update from `d583aba4f59840133fa1d78f6aa563a0bbd8930c` to `12c09d41ef9c9b163f54c5c43fa7ae5c3038f053` and Engine update from `42b038cf5ac60f3533e8a760505da95078a900c5` to `6307c716cda32ab857c7754be39130dd17d6ef46`, with eight named root/Engine `include-untracked` safety stashes retained through opening, closure, final, and post-final reconciliation.
- Independent reusable documentation for `.fofnt`, binary BMFont v3, source-image dependencies, raw-copy delivery, runtime slots, sprite/atlas/effect binding, scaling, borders, grayscale processing, inline colors, text layout flags, hit testing, diagnostics, and embedding-project responsibilities.
- Complete reconciliation of the incoming package DSL and Windows 7 build lanes, the atomic mesh-only/Ozz model subsystem, and the latest Last Frontier weapon-event, Gambell intro, transferable-loot, platform-build, raider-hunt, campaign-hunt recovery, map-feedback, death-movement, gameplay-test, and synchronization changes. Project behavior remains in Last Frontier owners; reusable packaging, model, and font behavior remain in Engine owners.

Source areas checked:

- `FontManager`, font script methods and enums, sprite and atlas dependencies, effect binding, resource loading, settings, built-in tests, and Last Frontier font assets, GUI bindings, scale settings, localization integration, and visible PDA use.
- `DefinePackage`, package generation, archive and WiX tests, CMake build stages, Windows architecture normalization, `check_windows7_imports.py`, and the complete final Engine range.
- `ModelSourceLoader`, `ModelAnimationConverter`, `ModelMeshData`, `ModelAnimationData`, `ModelManager`, `ModelHierarchy`, `ModelInformation`, `ModelInstance`, `ModelAnimation`, their focused native tests, and the complete post-final 3D/Ozz range.
- The complete final Last Frontier range, including weapon snapshots, intro scene progression, medical-history loot, Windows 7 CI/package integration, world-raider contracts, and the production plus testing-callback synchronization gates.

Results:

- Added [FontFormat.md](FontFormat.md), `BuildTools/FontFormatInterface.json`, `BuildTools/docs_font_format.py`, [generated/font-format.json](generated/font-format.json), eight generated reference pages, and nine source-backed documentation tests.
- The generated font model contains 57 contract entries, 13 `.fofnt` fields, and 9 BMFont rules. It distinguishes `.bmfc` authoring sidecars from runtime inputs, records signed BMFont metrics, derives the live `FontType` slots and layout flags, and keeps project typography and GUI composition outside the reusable contract.
- Added `font-format` as the fourteenth aggregate contract-diff domain and routed it through CI, standalone validation, manifest, navigation/search, AI delivery, generated-reference maintenance, public API notes, production planning, backlog, and Last Frontier integration routes.
- The source audit fixed two font-loader defects: empty `.fofnt` names are rejected before suffix inspection, and signed BMFont offsets/advances are preserved instead of being decoded as unsigned values. A visible Direct3D PDA check confirms regular, bordered, and scaled `Big` text; the local capture is `Workspace/FontDocsLive/pda.png`.
- Reconciled the live package contract to per-`BINARY POSTFIX`, removed the obsolete package-wide postfix option from the machine model, documented `win32-win7` and `win64-win7`, and added the PE import checker to the generated helper-CLI reference. Focused CMake tests prove that each binary postfix is stored independently.
- The exact preserved-baseline aggregate diff reports 15 changes across 14 domains: 9 package changes, 2 helper-CLI changes, and 4 model-format changes, with all 3 required migration dispositions satisfied and 0 missing dispositions.
- Last Frontier integration docs now route reusable font mechanics to the Engine owner while retaining project font names, scales, GUI placement, Russian-first localization, and visible acceptance policy. Final callback audits also drove explicit synchronization reproofs in the touched combat, faction-contract, quest, and intro tests rather than accepting new baseline debt.
- The post-final model reconciliation replaces deleted `3dStuff` / `3dAnimation` anchors with the live source owners and documents the coordinated `LFMODMSH` mesh, `LFMODINF` description, and required `LFOZZRIG` rig boundary. Direct FBX runtime fallback is not promised; `AllowAnimationGeometry` is documented as a temporary validation-only source-repair exception, and model/particle manifests, maintenance routes, generated references, site data, and AI delivery now follow the current modules.
- Repeated synchronization checks exposed late lifecycle defects rather than documentation drift. `Combat::ApplyDamage` now stops after a damage callback destroys its target, `ToDeadWithImpulse` also rejects destroying corpses, lethal-damage tests strictly re-prove their corpse handles, and embedded-client tests reacquire the current controlled critter after destructive reset helpers. Production baseline keys decreased by eight with one count shrinking from three to one; the callback baseline decreased by four. No new diagnostic was accepted.

Mechanical checks:

- Focused font documentation tests pass 9/9. Complete `test_docs*.py` discovery passes 189 tests; `docs_validate.py` validates 147 maintained Markdown entries.
- All fourteen structured domains, API/reference/inventory, site, and AI artifacts are current. The API model contains 941 methods, 133 properties, 121 events, 266 settings, and one explicit contract; the site model contains 77 navigation items, 139 searchable documents, 141 public routes, and 135 planned redirects. AI delivery contains 141 public documents.
- Package and Windows 7 tests pass 24 tests with one platform-dependent skip; all three CMake interface-contract validation projects pass. Python generator checks and the exact aggregate contract-diff gate pass.
- Last Frontier `CompileAngelScript` and `ForceBakeResources` complete cleanly for version `0.3.560`, including 65 model descriptions and all 612 maps; a following ordinary bake reports zero stale outputs in every category. `LF_Client`, `LF_ServerHeadless`, and `LF_UnitTests` rebuild cleanly on the Ozz/meshoptimizer toolchain. Focused model/Ozz coverage passes 63 test cases and 5,334 assertions; the complete native suite passes all 403 test cases and 361,305 assertions.
- Production synchronization analysis covers 6,932 functions and accepts all 1,650 diagnostics with zero new or stale entries. The isolated callback gate covers 10,393 functions and accepts all 4,856 diagnostics with zero new or stale entries; all 122 analyzer tests pass. The exception-safety audit checks 5,453 functions with zero errors or warnings, and the smart-pointer audit checks 463 files with no diagnostics.
- The complete AiControl MCP suite passes 1,672 tests plus 57 subtests and its static smoke gate passes. Seven focused gameplay cases pass with zero failures, timeouts, skips, or global exception delta: both death-movement scenarios, map-feedback look-distance and F5 embedded-client probes, Lendale Butchers, stale intro completion, and direct-damage event delivery. The earlier broad reconciliation evidence remains recorded above, while these focused runs are the authoritative post-final-refresh checks.
- Ruby and Bundler are unavailable on this host, so local Liquid rendering could not run. GitHub Actions `jekyll-build-pages` remains the authoritative site-render and publication gate.

Follow-up:

- Continue the remaining reusable authoring program with GUI, dialogs, audio/video, and focused editor manuals from their owning parsers, runtime modules, tools, and tests.
- Publish the first owner-approved repository from [PublicExampleRepositories.md](PublicExampleRepositories.md), then use it to validate the minimal font/localization/GUI path against an exact Engine revision and provenance-reviewed assets.
- Confirm the landed fourteen-domain diff, documentation-site, AI-delivery, package/Win7, native/project, and `jekyll-build-pages` artifacts in GitHub Actions; production Pages source confirmation remains administrator work.

## 2026-07-19 - small-vector guidance and dependency-order reconciliation

Scope:

- Safe Last Frontier update from `12c09d41ef9c9b163f54c5c43fa7ae5c3038f053` through `e1fac76d5fb1c5486327f5a244fcfec343c99f0d` and `ee7bd89a7d19449c64b4dce6b9290356e95f12d0` to `c7f2c93247f283fa2a06acea4a55855c2ba3170a`, and Engine update from `6307c716cda32ab857c7754be39130dd17d6ef46` through `c01dbd04a1830d7ab5968fbc011128a48abe6a86` to `12e63fc4c4730719b7f4123ecdbdd783cda675fb`.
- Reconcile the Engine `small_vector` adoption, overlay repack-capacity correction, and baker output dependency ordering without promoting Last Frontier behavior into reusable contracts.
- Reconcile the project PlayerStart, personal-room terminal, open-hunt, Headhunters defection/recruiter routing, dynamic encounter, and Rvach movement-animation changes with their owning project documentation and tests.

Results:

- [Essentials.md](Essentials.md) now owns `vector` versus `small_vector` selection, measured inline capacity, address invalidation on inline moves, exact-type boundaries, complete-element requirements, nested-NSDMI constraints, formatter behavior, exception caveats, and adoption validation. [Debugging.md](Debugging.md) documents the automatically wired `small_vector.natvis`; [ExceptionSafety.md](ExceptionSafety.md) distinguishes terminate-on-OOM allocation from element construction and move failures.
- `Test_Containers.cpp` now proves allocator identity, inline-to-heap growth, `inlined()` state, inline relocation, heap-buffer transfer, mixed-mode swap, and formatting. The focused `Containers` and `CommonHelpers` cases pass 25 and 46 assertions respectively.
- The incoming [EntityModel.md](EntityModel.md) correction records that overlay capacity must be re-evaluated after a repack changes the aligned tail. The incoming [BakingPipeline.md](BakingPipeline.md) correction records dependency-order output discovery, later-pack replacement, and reverse-priority file resolution. Both claims have focused native regressions in their owning source areas.
- Last Frontier `Docs/AiControl.md` now records that open hunts may match semantic quest roles without a fixed map prototype, search authored spawn waypoints on dynamic encounter maps, patrol relative to the target territory, explore before exiting, and preserve remembered maximum health while recovering a wounded target. It also owns the Headhunters recruiter routes, four-quest raider-hunt family, headquarters exclusion, and proactive-combat suppression during peaceful approaches; these remain project-runner behavior rather than reusable Engine contracts. The Rvach asset fix remains project-owned and is documented by its dedicated implementation plan.
- Full callback analysis exposed retained critter handles after destructive reset helpers in the terminal, PlayerStart, GUI initial-build, and new Rvach tests. Each callback now reacquires and re-locks the live entity instead of expanding the baseline. The project exception-safety baseline also re-derives the changed `BakerDataSource` constructor at its unchanged `Strong` level.

Mechanical checks:

- Complete `test_docs*.py` discovery passes 189 tests and standalone validation covers 147 maintained Markdown entries. All fourteen structured domains plus API, reference, inventory, site, route, and AI-delivery artifacts pass their freshness checks. The preserved-baseline aggregate diff remains 15 changes across 14 domains, with all 3 required dispositions present and 0 missing.
- Last Frontier `CompileAngelScript` passes on version `0.3.564`. `BakeResources` completes a full rebuild with 65 model descriptions and all 612 maps, then an ordinary bake reports zero outputs in every category. Both Rvach model reports contain `animation_data_issues=0`.
- The complete rebuilt native suite passes 406 test cases and 361,348 assertions. This includes the new overlay-growth case and both baker dependency/priority cases in addition to the focused container coverage.
- Production synchronization remains clean at 6,936 functions and 1,650 accepted diagnostics; the latest isolated callback gate covers 10,414 functions and accepts 4,846 diagnostics against 3,363 baseline entries, with zero new or stale diagnostics. All 122 analyzer self-tests pass. Exception-safety checks 5,453 functions with zero errors or warnings, and SmartPointerAudit checks 463 files with no diagnostics.
- The complete AiControl suite passes 1,678 tests and its static smoke gate passes. The latest two regressions cover Headhunters recruiter-to-hunt routing and peaceful-approach attack suppression. The Rvach embedded-client regression passes `idle=true walk=true run=true`; the immediately preceding reconciliation also passes two client terminal/initial-build scenarios and three PlayerStart exit scenarios, all with zero failures, timeouts, skips, or global exception delta.
- Final fetch parity is `0/0` for Last Frontier `c7f2c93247f283fa2a06acea4a55855c2ba3170a` and Engine `12e63fc4c4730719b7f4123ecdbdd783cda675fb`; the root gitlink matches Engine HEAD, staged and unmerged sets are empty, and `git diff --check` passes in both repositories. All thirteen safety stashes created by this documentation pass were removed while the four unrelated pre-existing stashes remain untouched.
- Ruby and Bundler remain unavailable on this host. GitHub Actions `jekyll-build-pages` is the authoritative Liquid render and publication gate for the Markdown site at `fonline.ru`.

Follow-up:

- Keep the broader `small_vector` gameplay matrix and before/after Tracy allocation captures open until they can run on a stable profiling host; the reusable semantic, native, audit, bake, and focused gameplay gates are complete.
- Continue the source-backed authoring program with GUI, dialogs, audio/video, and focused editor manuals, then publish the first owner-approved exact-pin example repository.
- Confirm the landed documentation, contract diff, site, AI delivery, native/project, and `jekyll-build-pages` artifacts in GitHub Actions. Production Pages source confirmation remains administrator work.

## 2026-07-20 - polygonal-sprite reconciliation and private example staging

Scope:

- Safe Last Frontier update from `c7f2c93247f283fa2a06acea4a55855c2ba3170a` to `c97c0a993aa1170afd26272cf421067c18ed29fd` across nine commits, and Engine update from `12e63fc4c4730719b7f4123ecdbdd783cda675fb` to `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42` for `Polygonal sprites (#187)`.
- Complete reconciliation of the incoming sprite/model resource, baking, rendering, atlas, dependency, and setting contracts with the standalone Engine documentation corpus and minimal project.
- Administrator-authorized creation and initial private staging of the four planned `cvet/fonline-*` example repositories without claiming public release.

Results:

- [ImageFormat.md](ImageFormat.md), [ModelFormat.md](ModelFormat.md), [ModelAnimation.md](ModelAnimation.md), [BakingPipeline.md](BakingPipeline.md), [FrontendAndRendering.md](FrontendAndRendering.md), [SourceTree.md](SourceTree.md), and [ThirdPartyMaintenance.md](ThirdPartyMaintenance.md) now cover SpriteResource v2, per-frame offsets and polygon meshes, SpriteInfo indexing, automatic model bounds/layout, renamed animation metadata, atlas diagnostics, and the Clipper2/earcut ownership boundary. Their structured models and source anchors were reconciled with the incoming source.
- The first exact-pin template smoke found that a standalone config did not materialize a valid `SpriteMesh.MaxTriangles` without explicit values. `Examples/MinimalProject/FOnlineStarter.fomain` now records all four disabled defaults, the format/baking guides document unconditional group validation, and focused tests prevent the scaffold from dropping them.
- The same smoke found that the candidate runner imported the locally developed but not-yet-pinned `BuildTools/docs_metadata.py`. The runner now strictly decodes the small paired metadata contract itself, checks both required calls on server/client evidence, and safely relays logs through constrained Windows console encodings. The richer catalog generator remains the owning documentation tool without becoming a standalone runtime dependency.
- `Examples/PublicRepositories.json` now separates source lifecycle from remote visibility/state and rejects any private repository described as published. Generated output omits private repository URLs. `cvet/fonline-project-template` is private and source-staged at `9946ca42c332a294f8fedd2732e7850a01c1ec27`; the other three private repositories are reserved at `97d232431488125b370be352fdcf28f66e6cbf4f`, `011dab0d07eef6387609821206b8ee534ec51c3f`, and `97823816ab333a62aced43edd4daafa19c5fee22`.
- The first clean Ubuntu workflow selected stock Clang 18, below the incoming Engine minimum of Clang 20. The second reached CMake with supported stock GCC but exposed the missing clean-host X11 package set. The Linux preset and focused tests now lock GCC 13+, while both governance workflows prepare versioned prerequisites through the checked-out Engine's own `prepare-workspace.sh linux-packages linux` command instead of duplicating package lists.

Mechanical checks so far:

- Model-format and image-format write/check passes complete with seven focused tests each; model-animation passes five focused tests. Public-example generation/check and its five focused tests pass with four private remotes, one source-ready candidate, and zero published repositories.
- The exact-pin external template validator passes for Engine `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42`. Full Windows `python validate.py` passes on template commit `a2cdc06`, including configure, baker/server build, resource bake, headless lifecycle and native markers, and paired baked remote-call metadata; current `9946ca42` preserves that route and adds the Linux preset, prerequisite workflow, and corresponding prose.
- All 189 documentation tests pass, standalone validation covers 147 Markdown entries, and generated API/inventory report 941 export methods, 267 settings, and 93 native test files. The complete rebuilt native binary passes 422 test cases and 424,528 assertions.
- GitHub API verification confirms all four repositories are private, use `main`, and point at the expected local commits. `Pinned Engine` run `29739863448` on `9946ca42` passed on `windows-latest` and `ubuntu-latest`; manual `Current Engine Compatibility` run `29740066760` passed on Ubuntu against Engine `master`. Private staging still does not satisfy the public branch/security/tag/artifact gates.
- Final fetch parity is `0/0` for Last Frontier `c97c0a993aa1170afd26272cf421067c18ed29fd` and Engine `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42`; the root gitlink matches Engine HEAD and unmerged sets are empty. Task safety stashes `25150f45` and `d064b811` were removed only after these checks, while the four older user stashes remain untouched.

Follow-up:

- Keep all repositories private until their recorded source, CI, branch/security, tag, artifact, and review gates pass. Populate the three reserved repositories in dependency order; do not turn reservation READMEs into public examples.
