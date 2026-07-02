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
- Symbol spot checks found the documented owners and APIs in current source, including `AddStageHook`, `AddExecutableApplication`, `AddSharedApplication`, `BakeResources`, `ForceBakeResources`, `CompileAngelScript`, `CompileManagedScripts`, `BaseBaker`, `SetupBakers`, `MasterBaker`, `MetadataBaker`, `CodeGeneration`, `ForceCodeGeneration`, `FO_SOURCE_META_FILES`, `FO_MANAGED_SOURCE`, `FO_ADDED_COMMON_HEADERS`, `FO_EMBEDDED_DATA_CAPACITY`, `FO_INTERNAL_CONFIG_CAPACITY`, `RegisterDynamicMetadata`, `MetadataRegistration` templates, `GenericCode-Template`, and `PropertyRegistrator`.
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

- `Source/Common/ScriptSystem.*`, `Source/Scripting/AngelScript/AngelScriptScripting.*`, `AngelScriptBackend.*`, `AngelScriptAttributes.cpp`, `AngelScriptCall.cpp`, `AngelScriptEntity.cpp`, `AngelScriptGlobals.cpp`, `AngelScriptRemoteCalls.cpp`, `AngelScriptReflection.cpp`, engine core scripts, Managed/Native roots, and `BuildTools/cmake/stages/ScriptsAndBaking.cmake` for scripting runtime/build flow.
- All 18 current `Source/Scripting/*ScriptMethods.cpp` files for native exported method ownership and current export counts.
- `Source/Scripting/AngelScript/AngelScriptAttributes.cpp`, `Source/Tools/MetadataBaker.cpp`, `BuildTools/codegen.py`, `Source/Common/ScriptSystem.h`, `Source/Essentials/BasicCore.h`, nullable analyzer tools under `../Tools/NullableEstimate/`, parent VS Code/CI task wiring, and nullable/script tests for nullability contracts.

Results:

- Confirmed the current native script method map: 874 `///@ ExportMethod` declarations across 18 script method files after the 2026-05-21 default-argument overload collapse.
- Corrected stale nullability workflow wording: current nullable appliers preserve author-chosen markers and remove redundant guards; they do not own automatic contract inference.
- Replaced stale parent docs routes in `Nullability.md` with current engine docs and the current `Source/Tests/README.md` testing source of truth.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `ScriptSystem`, `ScriptSystemBackend`, `RegisterBackend`, `MapScriptTypes`, `InitModules`, `FindFunc`, `CheckFunc`, `CallFunc`, `CallAdminFunc`, `NativeDataProvider`, `CheckArgNotNull`, `CheckReturnNotNull`, `InitAngelScriptScripting`, `CompileAngelScript`, `AngelScriptBackend`, `RegisterMetadata`, `CompileTextScripts`, `LoadBinaryScripts`, `StripNullableTypeSuffix`, `FO_NULLABLE`, and `is_validated_pointer_meta_type`.
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
- Embedding-project examples explicitly marked as examples: `../../Scripts/Managed/MapperRender.cs`, `../../Tools/MapPreview/generate_map_preview.py`, `../../Tools/MapPreview/map_preview_overrides.ini`, and `../../LastFrontier.fomain`.

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
