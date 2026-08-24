---
permalink: /Examples/NativeExtensionSample/README.ru.html
locale: ru
document_id: native-extension-sample-readme
---

# Пример native-расширения FOnline

<!-- docs-translation: {"document_id":"native-extension-sample-readme","locale":"ru","source_path":"Examples/NativeExtensionSample/README.md","source_sha256":"2b20525120004086efe0011fdd84e8deba8279546849a9e4907a715309b335dc"} -->

Этот принадлежащий движку проект показывает полный путь проектного C++-расширения без игровых сервисов и сторонних SDK. Он отделён от минимального starter-проекта, чтобы границы владения native-кодом и совместимости оставались явными.

Пример доказывает следующее:

- `SourceExt/ServerExtension.cpp` зарегистрирован в узкой роли `SERVER`;
- `ServerInitHook` создаёт состояние одного экземпляра `ServerEngine` и освобождает его через аллокатор движка;
- `Server_Game_NativeExtensionValue` становится методом `Game.NativeExtensionValue()` в серверном AngelScript;
- `NativeExtensionCore` подключается через текущий привязанный к ревизии список `FO_SERVER_LIBS`, а compile-time проверка подтверждает передачу usage requirement;
- `FONATIVE_NativeExtensionCoreTest` проверяет независимую от движка границу значений фиксированной ширины;
- `run_native_extension_smoke.py` проверяет lifecycle hook, сгенерированный script binding, чтение состояния и чистое завершение сервера.

## Запуск проверок

Инициализируйте `Engine/` на точной ревизии из метаданных репозитория и выполните:

```bash
python validate.py
```

Эквивалентные команды:

```bash
# Windows
cmake --preset windows
cmake --build --preset windows-check

# Linux
cmake --preset linux
cmake --build --preset linux-check
```

На Linux сначала установите зависимости, определённые закреплённой ревизией Engine:

```bash
Engine/BuildTools/prepare-workspace.sh linux-packages linux
```

Успех подтверждают native- и runtime-маркеры:

```text
native_extension_core_test_passed
native_extension_hook_initialized
native_extension_value=42
native_extension_smoke_passed
```

## Владение и граница ABI

Проект собирает расширение и Engine из исходников в одной сборке. Стабильный бинарный ABI между ревизиями Engine не обещается. При обновлении gitlink необходимо заново сконфигурировать, собрать и запечь проект, затем повторить pinned/current проверки совместимости в одной ревизии.

`NativeExtensionCore` передаёт focused-тесту только значения фиксированной ширины. Engine handles остаются в зарегистрированной единице трансляции и используют заимствования `ptr<T>`. Состояние отдельного сервера хранится в `ServerEngine.UserData`; изменяемых file-scope переменных нет. Реальный проект с несколькими native-подсистемами должен хранить в этом слоте один проектный агрегат, а не позволять независимым расширениям конкурировать за него.

`FO_SERVER_LIBS` является текущим привязанным к ревизии integration state, а не
helper-командой, объявленной в `BuildTools/cmake/ProjectInterface.json`;
проверяйте его заново при каждом изменении pin Engine.

В примере нет распространяемых материалов, runtime-библиотек, credentials и интеграций с сервисами. Добавляйте их только вместе с явным происхождением, платформенным контрактом, приёмкой пакета и владельцем безопасности.

## Связанная документация

- [Native-расширения](../../Docs/ru/how-to/native-extensions.md)
- [Проектные зависимости](../../Docs/ru/how-to/native-extensions/project-dependencies.md)
- [Обновление Engine](../../Docs/ru/how-to/migration/engine-upgrade.md)
- [Публичные репозитории примеров](../../Docs/ru/how-to/build/public-example-repositories.md)
