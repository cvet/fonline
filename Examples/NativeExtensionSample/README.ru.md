---
permalink: /Examples/NativeExtensionSample/README.ru.html
locale: ru
document_id: native-extension-sample-readme
---

# Пример native-расширения FOnline

<!-- docs-translation: {"document_id":"native-extension-sample-readme","locale":"ru","source_path":"Examples/NativeExtensionSample/README.md","source_sha256":"393210f4b00ca5b2051f62439563d96ca3347d60ca3f315b6d2db5f80c945f91"} -->

Этот принадлежащий движку проект показывает полный путь проектного C++-расширения без игровых сервисов и сторонних SDK. Он отделён от минимального starter-проекта, чтобы границы владения native-кодом и совместимости оставались явными.

Пример доказывает следующее:

- `SourceExt/ServerExtension.cpp` зарегистрирован в узкой роли `SERVER`;
- `ServerInitHook` создаёт состояние одного экземпляра `ServerEngine` и освобождает его через аллокатор движка;
- `Server_Game_NativeExtensionValue` становится методом `Game.NativeExtensionValue()` в серверном AngelScript;
- `NativeExtensionCore` подключается через `AddProjectLibraries`, а compile-time проверка отклоняет неверную маршрутизацию роли;
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

Ветка совместимости в `CMakeLists.txt` добавляет `NativeExtensionCore` напрямую в
`FO_SERVER_LIBS` только для старых ревизий Engine, в которых ещё нет
`AddProjectLibraries`. Поэтому закреплённый release-маршрут проверяет
документированную вспомогательную функцию, а маршрут текущего Engine остаётся
полезным во время её внедрения.

В примере нет распространяемых материалов, runtime-библиотек, credentials и интеграций с сервисами. Добавляйте их только вместе с явным происхождением, платформенным контрактом, приёмкой пакета и владельцем безопасности.

## Связанная документация

- [Native-расширения](../../Docs/ru/how-to/native-extensions.md)
- [Проектные зависимости](../../Docs/ru/how-to/native-extensions/project-dependencies.md)
- [Обновление Engine](../../Docs/ru/how-to/migration/engine-upgrade.md)
- [Публичные репозитории примеров](../../Docs/ru/how-to/build/public-example-repositories.md)
