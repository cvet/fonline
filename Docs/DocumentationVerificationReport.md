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

- `Source/Common/NetBuffer.*`, `Source/Common/NetCommand.*`, and `Source/Common/NetworkUdp.*` for message framing, hash/debug-hash serialization, admin command packing, and ordered UDP behavior.
- `Source/Client/NetworkClient*` and `Source/Server/NetworkServer*` for transport-neutral client/server abstractions plus interthread, socket, UDP, ASIO, and WebSocket implementations.
- `Source/Server/Server.*`, `Source/Server/EntityManager.*`, `MapManager.*`, `CritterManager.*`, `ItemManager.*`, `Player.*`, `Critter.*`, `Map.*`, `Location.*`, `Item.*`, `ClientDataValidation.*`, and `UpdaterBackend.*` for authoritative server runtime ownership, entity/session flow, validation, managers, movement, persistence handoff, and updater hosting.
- `Source/Tests/Test_NetworkUdp.cpp`, `Source/Tests/Test_NetworkClient.cpp`, `Source/Tests/Test_NetworkServer.cpp`, `Source/Tests/Test_ServerEngine.cpp`, `Source/Tests/Test_ServerItems.cpp`, `Source/Tests/Test_ServerMapOperations.cpp`, `Source/Tests/Test_ServerAdvancedOps.cpp`, `Source/Tests/Test_ServerScriptMethods.cpp`, `Source/Tests/Test_ClientServerIntegration.cpp`, `Source/Tests/Test_ClientDataValidation.cpp`, and `Source/Tests/Test_DataBase.cpp` for the current validation surfaces named by the promoted pages.

Results:

- Added a `Source paths inspected` section to `Docs/Networking.md` and replaced its generic integration-test wording with the concrete current `Source/Tests/Test_ClientServerIntegration.cpp` reference.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `NetBuffer`, `NetOutBuffer`, `NetInBuffer`, `PackNetCommand`, `NetworkClientConnection`, `NetworkServerConnection`, `NetworkServer`, `UdpOrderedChannel`, UDP packet helpers, `ServerEngine`, server init/job methods, server events, `EntityManager`, `MapManager`, `CritterManager`, `ItemManager`, `Player`, inbound `Process_*` handlers, client-data validation functions, movement helpers, and `UpdaterBackend`.
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
- `Source/Client/ClientRuntimeApi.*`, `Source/Client/Updater.*`, `Source/Common/Common.h`, and `Source/Common/Settings-Include.h` for runtime ABI, updater protocol versioning, update targets, disk hashing/cache behavior, and updater settings.
- `Source/Server/UpdaterBackend.*` and `Source/Server/Server.cpp` for server-side descriptor generation, file serving, target-specific binaries, and `UpdateFileMaxPortionSize` use.
- `BuildTools/cmake/stages/Applications.cmake` and `BuildTools/package.py` for client host/library build gates and runtime binary packaging/staging.
- `Source/Tests/Test_ClientRuntimeApi.cpp` plus updater pipeline tests under `../Tools/PipelineTests/` for runtime ABI, binary update, staging promotion, resource pack, compatibility-outdated, updater-outdated, missing-payload, and smoke coverage.

Results:

- Added a `Source paths inspected` section to `Docs/ClientUpdater.md`.
- Replaced obsolete package-entry wording with the current `build_runtime_update_target_name` owner in `BuildTools/package.py`.
- Backticked source/build/doc path checks for this slice: no missing paths.
- Symbol spot checks found the documented owners and APIs in current source, including `UpdaterBackend`, `LoadFromClientResources`, `ProcessUpdateFile`, `GetUpdateDescriptor`, `FO_CLIENT_RUNTIME_HOST_ABI_VERSION`, `ClientRuntimeMetadata`, `ClientRuntimeExports`, `ClientRuntimeResult`, `FO_QueryClientRuntimeExports`, `ApplyStagedBinaryUpdate`, `GetClientRuntimeLivePath`, `GetClientRuntimeStagingPath`, `RunClientFromLibrary`, `RunEmbeddedOrLoadedClient`, `ResolveRequestedClientRuntime`, `CanSelfUpdateNativeModules`, `FO_UPDATER_VERSION`, `UpdateFileTarget`, `ClientBinaries`, `ClientResources`, `GetCurrentBinaryUpdateTargetName`, `UpdateFileMaxPortionSize`, `UpdateFilesInMemory`, `PlatformBinaries`, and `build_runtime_update_target_name`.
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

- Continue with semantic verification of `Docs/BuildToolsPipeline.md`, `Docs/BakingPipeline.md`, and `Docs/GeneratedApiAndMetadata.md`.
