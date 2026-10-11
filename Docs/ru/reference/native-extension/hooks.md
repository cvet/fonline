---
title: Хуки движка
document_id: generated-native-extension-hooks
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-native-extension-hooks","locale":"ru","source_path":"Docs/en/reference/native-extension/hooks.md","source_sha256":"0a4cb39b7625651e4fefc05935b925b6766a54d21d0257f4c7a698456da85f71"} -->

# Хуки движка

> Сгенерированный справочник. Не редактируйте его напрямую. Обновите `BuildTools/NativeExtensionInterface.json`, затем выполните `python BuildTools/docs_native_extension.py --write`.

[Обзор](index.md) | [Роли](roles.md) | [Хуки](hooks.md) | [Биндинги](bindings.md) | [Канонический JSON](../../../generated/native-extension.json) | [Руководство](../../how-to/native-extensions.md)

Проект реализует хук, объявляя его как метаданные в исходнике, зарегистрированном во владеющей роли. Обнаружив объявление, кодогенерация исключает реализацию этого хука по умолчанию.

<a id="entry-native-extension-hook-applicationinithook-6c995d4e84"></a>
## `ApplicationInitHook`

Проверяет или корректирует настройки запуска приложения после инициализации платформы.

- Стабильный ID: `native-extension.hook.ApplicationInitHook`
- Роль: `COMMON`
- Наличие входит в хеш совместимости: `yes`
- Места вызова: [Source/Frontend/ApplicationInit.cpp](https://github.com/cvet/fonline/blob/master/Source/Frontend/ApplicationInit.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void ApplicationInitHook(AppInitFlags flags, GlobalSettings& settings);
FO_END_NAMESPACE
```

По умолчанию: Ничего не делает перед продолжением инициализации среды выполнения приложения.

<a id="entry-native-extension-hook-applicationshutdownhook-29f83f6c3a"></a>
## `ApplicationShutdownHook`

Освобождает принадлежащие проекту общепроцессные сервисы при завершении приложения.

- Стабильный ID: `native-extension.hook.ApplicationShutdownHook`
- Роль: `COMMON`
- Наличие входит в хеш совместимости: `no`
- Места вызова: [Source/Applications/ClientApp.cpp](https://github.com/cvet/fonline/blob/master/Source/Applications/ClientApp.cpp), [Source/Applications/ClientLib.cpp](https://github.com/cvet/fonline/blob/master/Source/Applications/ClientLib.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void ApplicationShutdownHook();
FO_END_NAMESPACE
```

По умолчанию: Ничего не делает во время защищённого завершения клиента или приложения.

<a id="entry-native-extension-hook-serverinithook-9f02a4bdf2"></a>
## `ServerInitHook`

Инициализирует принадлежащие проекту серверные сервисы и пользовательские данные движка.

- Стабильный ID: `native-extension.hook.ServerInitHook`
- Роль: `SERVER`
- Наличие входит в хеш совместимости: `yes`
- Места вызова: [Source/Server/Server.cpp](https://github.com/cvet/fonline/blob/master/Source/Server/Server.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void ServerInitHook(ptr<ServerEngine> server);
FO_END_NAMESPACE
```

По умолчанию: Ничего не делает, и инициализация сервера продолжается.

<a id="entry-native-extension-hook-clientinithook-20f8786455"></a>
## `ClientInitHook`

Инициализирует принадлежащие проекту клиентские сервисы и пользовательские данные движка.

- Стабильный ID: `native-extension.hook.ClientInitHook`
- Роль: `CLIENT`
- Наличие входит в хеш совместимости: `yes`
- Места вызова: [Source/Client/Client.cpp](https://github.com/cvet/fonline/blob/master/Source/Client/Client.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void ClientInitHook(ptr<ClientEngine> client);
FO_END_NAMESPACE
```

По умолчанию: Ничего не делает, и инициализация клиента продолжается.

<a id="entry-native-extension-hook-clientstartupsettingshook-1f9a0bc1b2"></a>
## `ClientStartupSettingsHook`

Корректирует настройки запуска отдельного клиента, включая встроенные экземпляры нескольких клиентов.

- Стабильный ID: `native-extension.hook.ClientStartupSettingsHook`
- Роль: `COMMON`
- Наличие входит в хеш совместимости: `yes`
- Места вызова: [Source/Applications/ClientApp.cpp](https://github.com/cvet/fonline/blob/master/Source/Applications/ClientApp.cpp), [Source/Applications/ServerApp.cpp](https://github.com/cvet/fonline/blob/master/Source/Applications/ServerApp.cpp), [Source/Applications/ServerHeadlessApp.cpp](https://github.com/cvet/fonline/blob/master/Source/Applications/ServerHeadlessApp.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void ClientStartupSettingsHook(GlobalSettings& settings, int32_t client_index, bool embedded);
FO_END_NAMESPACE
```

По умолчанию: Ничего не делает перед запуском отдельного или встроенного клиента.

<a id="entry-native-extension-hook-setupbakershook-a30d9e9944"></a>
## `SetupBakersHook`

Добавляет принадлежащие проекту запекатели ресурсов, запрошенные активной конфигурацией запекания.

- Стабильный ID: `native-extension.hook.SetupBakersHook`
- Роль: `BAKER`
- Наличие входит в хеш совместимости: `yes`
- Места вызова: [Source/Tools/Baker.cpp](https://github.com/cvet/fonline/blob/master/Source/Tools/Baker.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API void SetupBakersHook(const_span<string> requested, vector<unique_ptr<BaseBaker>>& bakers, shared_ptr<BakingContext> context);
FO_END_NAMESPACE
```

По умолчанию: Не регистрирует проектные запекатели.

<a id="entry-native-extension-hook-checkcrittervisibilityhook-c0d19676e0"></a>
## `CheckCritterVisibilityHook`

Применяет проектную политику видимости между наблюдателем и целевым криттером.

- Стабильный ID: `native-extension.hook.CheckCritterVisibilityHook`
- Роль: `SERVER`
- Наличие входит в хеш совместимости: `yes`
- Места вызова: [Source/Server/MapManager.cpp](https://github.com/cvet/fonline/blob/master/Source/Server/MapManager.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API CritterVisibilityMode CheckCritterVisibilityHook(ptr<const ServerEngine> server, ptr<const Map> map, ptr<const Critter> observer, ptr<const Critter> target);
FO_END_NAMESPACE
```

По умолчанию: Возвращает CritterVisibilityMode::Full.

<a id="entry-native-extension-hook-checkitemvisibilityhook-ff63fc0529"></a>
## `CheckItemVisibilityHook`

Применяет проектную политику видимости между криттером-наблюдателем и предметом на карте.

- Стабильный ID: `native-extension.hook.CheckItemVisibilityHook`
- Роль: `SERVER`
- Наличие входит в хеш совместимости: `yes`
- Места вызова: [Source/Server/Critter.cpp](https://github.com/cvet/fonline/blob/master/Source/Server/Critter.cpp)

```cpp
FO_BEGIN_NAMESPACE
///@ EngineHook
FO_SCRIPT_API bool CheckItemVisibilityHook(ptr<const ServerEngine> server, ptr<const Map> map, ptr<const Critter> observer, ptr<const Item> item);
FO_END_NAMESPACE
```

По умолчанию: Возвращает true.
