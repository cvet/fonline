---
title: Engine Hooks
document_id: generated-native-extension-hooks
locale: en
generated: true
---

# Engine Hooks

> Generated reference. Do not edit directly. Update `BuildTools/NativeExtensionInterface.json`, then run `python BuildTools/docs_native_extension.py --write`.

[Index](index.md) | [Roles](roles.md) | [Hooks](hooks.md) | [Bindings](bindings.md) | [Canonical JSON](../native-extension.json) | [Guide](../../NativeExtensions.md)

A project implements a hook by declaring it as metadata in a source registered under the owning role. Codegen omits that hook's fallback when it sees the declaration.

<a id="entry-native-extension-hook-applicationinithook-6c995d4e84"></a>
## `ApplicationInitHook`

Inspect or adjust application startup settings after platform initialization.

Stable ID: `native-extension.hook.ApplicationInitHook`  
Role: `COMMON`  
Compatibility-hashed presence: `yes`  
Call sites: [Source/Frontend/ApplicationInit.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/ApplicationInit.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void ApplicationInitHook(AppInitFlags flags, GlobalSettings& settings);
FO_END_NAMESPACE
```

Default: No-op before application runtime initialization continues.

<a id="entry-native-extension-hook-applicationshutdownhook-29f83f6c3a"></a>
## `ApplicationShutdownHook`

Release project-owned process services during application shutdown.

Stable ID: `native-extension.hook.ApplicationShutdownHook`  
Role: `COMMON`  
Compatibility-hashed presence: `no`  
Call sites: [Source/Applications/ClientApp.cpp](https://github.com/cvet/fonline/blob/master/Source/Applications/ClientApp.cpp), [Source/Applications/ClientLib.cpp](https://github.com/cvet/fonline/blob/master/Source/Applications/ClientLib.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void ApplicationShutdownHook();
FO_END_NAMESPACE
```

Default: No-op during guarded client/application shutdown.

<a id="entry-native-extension-hook-serverinithook-9f02a4bdf2"></a>
## `ServerInitHook`

Initialize project-owned server services and engine user data.

Stable ID: `native-extension.hook.ServerInitHook`  
Role: `SERVER`  
Compatibility-hashed presence: `yes`  
Call sites: [Source/Server/Server.cpp](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void ServerInitHook(ptr<ServerEngine> server);
FO_END_NAMESPACE
```

Default: No-op while server initialization continues.

<a id="entry-native-extension-hook-clientinithook-20f8786455"></a>
## `ClientInitHook`

Initialize project-owned client services and engine user data.

Stable ID: `native-extension.hook.ClientInitHook`  
Role: `CLIENT`  
Compatibility-hashed presence: `yes`  
Call sites: [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void ClientInitHook(ptr<ClientEngine> client);
FO_END_NAMESPACE
```

Default: No-op while client initialization continues.

<a id="entry-native-extension-hook-clientstartupsettingshook-1f9a0bc1b2"></a>
## `ClientStartupSettingsHook`

Adjust per-client startup settings, including embedded multi-client instances.

Stable ID: `native-extension.hook.ClientStartupSettingsHook`  
Role: `COMMON`  
Compatibility-hashed presence: `yes`  
Call sites: [Source/Applications/ClientApp.cpp](https://github.com/cvet/fonline/blob/master/Source/Applications/ClientApp.cpp), [Source/Applications/ServerApp.cpp](https://github.com/cvet/fonline/blob/master/Source/Applications/ServerApp.cpp), [Source/Applications/ServerHeadlessApp.cpp](https://github.com/cvet/fonline/blob/master/Source/Applications/ServerHeadlessApp.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void ClientStartupSettingsHook(GlobalSettings& settings, int32_t client_index, bool embedded);
FO_END_NAMESPACE
```

Default: No-op before standalone or embedded client startup.

<a id="entry-native-extension-hook-setupbakershook-a30d9e9944"></a>
## `SetupBakersHook`

Append project-owned resource bakers requested by the active bake configuration.

Stable ID: `native-extension.hook.SetupBakersHook`  
Role: `BAKER`  
Compatibility-hashed presence: `yes`  
Call sites: [Source/Tools/Baker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/Baker.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void SetupBakersHook(const_span<string> requested, vector<unique_ptr<BaseBaker>>& bakers, shared_ptr<BakingContext> context);
FO_END_NAMESPACE
```

Default: Register no project-owned bakers.

<a id="entry-native-extension-hook-checkcrittervisibilityhook-c0d19676e0"></a>
## `CheckCritterVisibilityHook`

Apply project visibility policy between an observer and target critter.

Stable ID: `native-extension.hook.CheckCritterVisibilityHook`  
Role: `SERVER`  
Compatibility-hashed presence: `yes`  
Call sites: [Source/Server/MapManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Server/MapManager.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API CritterVisibilityMode CheckCritterVisibilityHook(ptr<const ServerEngine> server, ptr<const Map> map, ptr<const Critter> observer, ptr<const Critter> target);
FO_END_NAMESPACE
```

Default: Return CritterVisibilityMode::Full.

<a id="entry-native-extension-hook-checkitemvisibilityhook-ff63fc0529"></a>
## `CheckItemVisibilityHook`

Apply project visibility policy between an observer critter and map item.

Stable ID: `native-extension.hook.CheckItemVisibilityHook`  
Role: `SERVER`  
Compatibility-hashed presence: `yes`  
Call sites: [Source/Server/Critter.cpp](https://github.com/cvet/fonline/blob/master/Source/Server/Critter.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API bool CheckItemVisibilityHook(ptr<const ServerEngine> server, ptr<const Map> map, ptr<const Critter> observer, ptr<const Item> item);
FO_END_NAMESPACE
```

Default: Return true.
