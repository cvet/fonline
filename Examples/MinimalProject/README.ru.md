---
permalink: /Examples/MinimalProject/README.ru.html
locale: ru
document_id: minimal-project-readme
---

<!-- docs-translation: {"document_id":"minimal-project-readme","locale":"ru","source_path":"Examples/MinimalProject/README.md","source_sha256":"233b246e37b21a2dbf80ce3e672c6e76e4fef792b21548735aa80718b8521695"} -->

# Минимальный проект FOnline

Это принадлежащий движку исполняемый starter и проект для проверки в CI. Он
намеренно достаточно мал, чтобы описать его целиком:

- `CMakeLists.txt` компонует FOnline через публичные вспомогательные функции
  стадий и направляет одну серверную зависимость `INTERFACE` через
  `AddProjectLibraries`;
- `CMakePresets.json` предоставляет отдельные пресеты конфигурации и smoke-теста
  для Windows x64 и Linux GCC;
- `FOnlineStarter.fomain` объявляет один smoke sub-config и минимальные
  переиспользуемые пакеты ресурсов;
- `Scripts/Starter.fos` подписывается на событие запуска сервера и объявляет по
  одному remote call в каждом направлении;
- `StarterServerExtension.cpp` демонстрирует серверный нативный экспорт в
  скрипты и необязательный hook движка;
- `run_starter_smoke.py` проверяет таймаут, нативные и скриптовые маркеры
  жизненного цикла и парные запечённые метаданные remote call;
- `validate.py` выбирает поддерживаемый пресет хоста и запускает полный маршрут
  конфигурации, сборки и smoke-теста;
- `Engine/` предоставляется как ссылка или подмодуль validation wrapper либо
  checkout игрового проекта.

В проекте нет карт, криттеров, предметов, диалогов, GUI, аутентификации или
продуктовой политики. Они принадлежат последующим руководствам и публичным
демонстрационным репозиториям.

## Автоматизированный smoke-тест

Из корня движка:

```bash
python BuildTools/buildtools.py validate linux-starter-smoke
# Windows:
python BuildTools/buildtools.py validate win64-starter-smoke
```

BuildTools заново создаёт `Workspace/validation-project`, копирует туда этот
каталог, связывает его дочерний `Engine/` с текущим checkout движка,
конфигурирует и собирает headless-сервер и baker, запекает ресурсы, а затем
запускает `RunStarterSmoke`.

Для успеха журнал сервера должен дойти до каждого маркера, а процесс —
завершиться с нулевым кодом:

```text
starter_native_extension_value=42
starter_server_started
starter_smoke_passed
```

Нативный маркер доказывает, что исходник роли `SERVER` был скомпилирован в
`ServerLib`, его объявление `ExportMethod` попало в codegen, а запечённый
AngelScript вызвал сгенерированную привязку `Game.NativeStarterValue()` во
время выполнения. Та же единица трансляции требует
`FO_STARTER_PROJECT_DEPENDENCY=1`, поэтому компиляция также доказывает передачу
серверной цели `INTERFACE` через `AddProjectLibraries`. Visibility hook
подтверждает обнаружение необязательного hook и подавление fallback при
генерации кода. Самодостаточный runner отклоняет процесс, который работает
дольше 60 секунд, завершается без любого маркера или создаёт несовместимые
контракты `Metadata.fometa-server` и `Metadata.fometa-client`. Запечённые
результаты должны содержать `script.remote-call.server.StarterPing` и
`script.remote-call.client.StarterNotice`; это проверяет тот же формат
`MetadataBaker`, который потребляет более полный генератор каталога
`BuildTools/docs_metadata.py` на стороне движка, не создавая зависимости
отдельного примера от этого инструмента документации. Маршрут Windows x64 был
успешно выполнен 31 июля 2026 года. Оба маршрута, Windows и Linux,
зарегистрированы в `.github/workflows/validate.yml`; Linux нельзя описывать как
проверенный, пока его задание CI не станет зелёным.

## Намеренные ограничения

Starter не включает `Config` baker, потому что распространяемая конфигурация
требует полного явного набора настроек. Он также не импортирует все
`CoreScripts` движка: клиентские базовые модули зависят от принадлежащих проекту
настроек, enum и сгенерированных символов GUI. Это интеграционные контракты для
последующих примеров, а не скрытые предварительные требования starter.

## Ручной checkout проекта

Чтобы использовать scaffold без validation wrapper движка, скопируйте этот
каталог в новый репозиторий и добавьте FOnline в `Engine/`. Выполните
конфигурацию из корня проекта с подходящими платформе и хосту инструментами,
соберите `BakeResources` и `FOSTART_ServerHeadless`, затем запустите
сгенерированный headless-сервер с аргументами:

```text
-ApplyConfig <project-root>/FOnlineStarter.fomain -ApplySubConfig StarterSmoke
```

После инициализации `Engine/` отдельный маршрут пресетов выглядит так:

```bash
# Debian/Ubuntu host prerequisites (first Linux run only)
Engine/BuildTools/prepare-workspace.sh linux-packages linux

# Cross-platform wrapper (Windows or Linux)
python validate.py

# Equivalent explicit commands
# Windows
cmake --preset windows
cmake --build --preset windows-smoke

# Linux
cmake --preset linux
cmake --build --preset linux-smoke
```

Workflows репозитория запускают ту же принадлежащую движку команду подготовки
Linux перед проверкой, поэтому набор системных пакетов соответствует
зафиксированной ревизии и не дублируется в YAML примера.

Пресет Windows намеренно оставляет выбор генератора CMake: CMake выбирает самую
новую установленную Visual Studio, тогда как фиксация `Visual Studio 17 2022`
отклонила бы совместимые более новые версии. Пресеты собирают только
headless-сервер и baker, необходимые для `RunStarterSmoke`; принадлежащие
проекту пресеты клиента, mapper, редактора, упаковки и развёртывания относятся
к последующим примерам. Не переименовывайте идентификаторы исходников и
конфигурации по одному. Изменяйте `FO_DEV_NAME`, `FO_NICE_NAME`, имя файла
`.fomain` и идентификаторы пакета или приложения вместе как одну операцию
начальной настройки проекта.

Проверенный первый урок —
[Первый headless-проект FOnline](../../Docs/ru/tutorials/first-project.md).
