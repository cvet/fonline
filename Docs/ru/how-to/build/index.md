---
layout: default
title: Процесс сборки
locale: ru
document_id: build-workflow
permalink: /Docs/ru/how-to/build/
---

# Процесс сборки

<!-- docs-translation: {"document_id":"build-workflow","locale":"ru","source_path":"Docs/en/how-to/build/index.md","source_sha256":"3c5210f482fd03630c1c3cd9ed6913f08b134cad1c069d4212a70703054ba15b"} -->

Этот документ объясняет, как работать со сборками FOnline, не перенося
предположения одного проекта в другой.

## Проверенные исходные пути

- `../BuildTools/README.md`
- `../BuildTools/Init.cmake`
- `../BuildTools/validate.sh`
- `../BuildTools/validate.cmd`
- `../BuildTools/buildtools.py`
- `../BuildTools/docs_cli.py`
- `Docs/ru/reference/buildtools/index.md`
- `../BuildTools/PackageInterface.json`
- `../BuildTools/docs_package.py`
- `ru/reference/packages/index.md`
- `../Examples/MinimalProject/`
- `../Examples/MinimalMultiplayer/`
- `../BuildTools/cmake/stages/Init.cmake`
- `../BuildTools/cmake/stages/ProjectOptions.cmake`
- `../BuildTools/cmake/stages/EngineSources.cmake`
- `../BuildTools/cmake/stages/Codegen.cmake`
- `../BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `../BuildTools/cmake/stages/Applications.cmake`
- `../BuildTools/cmake/stages/Packages.cmake`
- `../BuildTools/cmake/stages/Finalize.cmake`
- `../BuildTools/cmake/helpers/*.cmake`
- `../Source/Applications/TestingApp.cpp`
- `../Source/Tests/README.md`

## Используйте игровой проект как корень сборки

Обычно FOnline собирается через репозиторий игры, в который движок подключен
как `Engine/`. Выполняйте configure и build из корня игры, если конкретная
engine-only команда не требует иного.

Причины:

- имена targets задает проект;
- `.fomain` управляет конфигурацией конкретной игры;
- generated scripting API зависит от проекта;
- имена пакетов, signing, ресурсы и deployment settings принадлежат продукту;
- platform presets обычно находятся в `CMakePresets.json` игрового проекта.

## Обычный процесс

1. Откройте корень репозитория игры.
2. Проверьте доступные presets через CMake или IDE-интеграцию проекта.
3. Настройте самый узкий preset, покрывающий изменение.
4. Соберите минимальный подходящий target.
5. Выполните соответствующий test, package или launch target.
6. Обновите документацию, если процесс или поведение изменились.

## Первая сборка, принадлежащая движку

В репозитории есть одно стабильное исключение из проектных имен targets:
[Examples/MinimalProject](../../../../Examples/MinimalProject/README.ru.md).
Пример доказывает чистый headless-путь встраивания без Last Frontier, TLA или
другого игрового checkout.

Из корня движка выполните validation target своей host-платформы:

```powershell
cd Examples\MinimalProject
python validate.py
```

```bash
cd Examples/MinimalProject
python3 validate.py
```

Оба маршрута настраивают и собирают baker и headless server, выполняют baking
минимального AngelScript-проекта, запускают сервер без networking с in-memory
database и требуют lifecycle markers из руководства
[Первый headless-проект FOnline](../../tutorials/first-project.md). Закрепленные
Windows- и Linux-lanes проверены в CI.

Следующий маршрут движка собирает desktop client, headless client, headless
server и baker, затем проверяет metadata, content, login, загрузку карты,
локализованный текст, remote calls и replicated state:

```powershell
cd Examples\MinimalMultiplayer
python validate.py
```

```bash
cd Examples/MinimalMultiplayer
python3 validate.py
```

Исходники и ручной запуск описаны в
[Minimal Multiplayer](../../../../Examples/MinimalMultiplayer/README.ru.md) и
[Первом игровом клиенте](../../tutorials/first-client.md).

## Предварительные требования

Перед тем как превращать build profile в заявление о поддержке релиза,
проверьте [матрицу поддержки](../../reference/platforms/support-matrix.md). Generated matrix различает
обязательную компиляцию, исполняемое smoke evidence и source-only profiles;
приемка device, renderer, package, service и store остается ответственностью
проекта.

Точный набор зависит от host OS и целевой платформы, но обычно нужны:

- Git;
- CMake;
- Python 3;
- compiler/toolchain с поддержкой C++20;
- platform SDK для собираемых targets;
- Visual Studio или Build Tools для Windows-процессов;
- Emscripten и Node.js для Web;
- JDK и Android NDK для Android.

Предпочитайте инструкции игрового проекта: он может закреплять конкретные
версии SDK и инструментов.

### Контур совместимости с Windows 7

Build-platform keys `win32-win7` и `win64-win7` являются native Windows MSVC
lanes с toolset `v143,version=14.44`; на не-Windows host они завершаются сразу.
`FO_BINARY_OUTPUT_POSTFIX` задает независимую identity сборки и не следует из
суффикса `-win7` в имени платформы. Если проект собирается, например, с
`Win7`, соответствующая package declaration должна использовать то же значение
только в этой записи: `BINARY Client Windows win32-win7 Raw+Zip+Wix POSTFIX Win7`.

До packaging или публикации проверьте каждый связанный EXE и DLL:

```powershell
python BuildTools/check_windows7_imports.py <client.exe> <client-runtime.dll>
```

Проверка разбирает PE imports и отклоняет `CreateFile2`, текущий запрещенный
Windows 8+ import. Конкретная установка toolset, binary paths, package matrix и
CI gate принадлежат игровому проекту; переиспользуемое правило проверки
определяет [Тестирование](../../contributing/testing/).

## Где находится логика сборки

Точные основные команды, arguments, defaults, choices и исполняемый help
приведены в generated
[справочник BuildTools CLI](../../reference/buildtools/index.md).

Generated [package interface reference](../../reference/packages/index.md)
определяет grammar `DefinePackage`, допустимые targets/platforms/architectures,
совместимость pack tokens, payload layouts и output artifacts. Порядок
build/bake/package, platform procedures, artifact evidence, signing, acceptance
и recovery boundaries описаны в
[упаковке и выпуске](../release/packaging.md). Конкретная package
matrix игры остается в документации этой игры.

Для library, SDK, framework или runtime payload, принадлежащего игровому
репозиторию, следуйте [Project-Local Dependencies](../../../ProjectDependencies.md).
Создайте project CMake target, добавьте его в самый узкий потребляемый список
`FO_*_LIBS`, поддерживаемый закреплённой ревизией, и проверьте и compiled feature state, и packaged
runtime state.

- [Обзор BuildTools](../../../../BuildTools/README.ru.md).
- [конвейер BuildTools](../../reference/cmake-and-buildtools/pipeline.md) описывает staged CMake
  pipeline и маршрутизацию изменений.
- `../BuildTools/cmake/` содержит reusable CMake modules и staged
  generation/build/package logic.
- `../BuildTools/Init.cmake` является project-facing CMake entry point и строгим
  stage dispatcher.
- Корень игрового проекта владеет product presets, configuration и выбором targets.

## Проверка по типу изменения

Если меняется `BuildTools/buildtools.py::create_parser()`, сначала
перегенерируйте и проверьте CLI model/pages, затем проверяйте затронутую команду
в реальном игровом проекте.

Если меняются package declarations или payload behavior, обновите
`BuildTools/PackageInterface.json`, перегенерируйте и проверьте model/pages,
запустите `validate_package_interface.cmake` и `test_packaging_matrix.py`, затем
соберите `RunPackagingChecks`, `RunTutorialPackageChecks` или более узкий product
package target из владеющего примера или проекта. Эти example targets являются
необязательными и не входят в обязательный реестр проверок Engine. Engine
fixtures доказывают native raw/archive/config/updater mechanics, но не заменяют
signing, install, deployment или rollback lane игры.

- **Runtime C++:** соберите и запустите project unit-test target; выбрать
  focused suites и понять generated test targets поможет
  [Тестирование](../../contributing/testing/).
- **CMake/BuildTools:** повторите configure из чистого или подходящего build
  directory и выполните затронутый build/package target; stage ownership
  описан в [конвейере BuildTools](../../reference/cmake-and-buildtools/pipeline.md).
- **Generated API:** пересоберите generation targets, проверьте компиляцию
  scripts и [Generated API and Metadata](../../reference/metadata/index.md).
- **Project config/resource packs:** следуйте
  [Конфигурации игрового проекта](project-configuration.md), компилируйте scripts,
  выполняйте обычный baking, forced baking при изменении input graph и запускайте
  consuming sub-config.
- **Generated outputs:** соблюдайте dependency order из
  [Generated Content Workflow](generated-content.md), не редактируя build-tree,
  baked или documentation artifacts.
- **Engine pin:** следуйте [руководству по обновлению Engine](../migration/engine-upgrade.md),
  включая аудит полного диапазона, generated contract comparison,
  persistence/network/updater review и reconciliation документации.
- **Resource baking:** выполните подходящий normal/forced bake и обратитесь к
  [Baking Pipeline](../../explanation/content-pipeline/baking.md).
- **Updater:** [Client Updater](../../explanation/runtime/client-updater.md).
- **Web:** [сборка, упаковка и отладка в браузере](../platforms/web-debugging.md).
- **Android:** [сборка, упаковка и отладка на Android](../platforms/android-debugging.md).
- **Mapper/tooling:** [Tools](../../../Tools.md) и
  [инструменты Mapper](../tools/mapper.md).
- **AngelScript source/refactor:** следуйте
  [Стилю AngelScript и рефакторингу](../scripting/style-and-refactoring.md), запускайте wrapper движка
  или проекта, компилируйте все затронутые стороны без warnings и выполняйте
  самый узкий behavior/contract test.
- **Nullability/script boundary:** [Scripting](../../explanation/scripting-runtime/),
  [Script Methods Map](../../reference/script-api/method-ownership.md) и
  [Nullability](../../contributing/coding-contracts/nullability.md).
- **Configuration/resources:**
  [Конфигурация и источники данных](../../reference/settings/configuration-and-data-sources.md) и
  [Baking Pipeline](../../explanation/content-pipeline/baking.md).
- **Essentials/low-level utilities:** [Essentials](../../reference/native/essentials.md) и
  соответствующие tests из [Тестирования](../../contributing/testing/).

## Поддерживаемость документации сборки

Не копируйте полный список presets в документацию движка. Presets меняются
между играми и branches. Объясняйте ownership и ссылайтесь на проектный
документ, которому принадлежат точные команды.

## Контрольный список

1. До публикации точного имени убедитесь, что command или preset принадлежит
   игровому проекту.
2. Для BuildTools changes повторите configure самого узкого preset и выполните
   generated target, использующий измененный stage.
3. Для runtime changes сначала выполните focused tests, затем project
   `RunUnitTests`, когда это практично.
4. Для package/platform changes обновите владеющий package/debug document в том
   же изменении.
5. При изменении самого build workflow обновите
   [конвейер BuildTools](../../reference/cmake-and-buildtools/pipeline.md),
   [Тестирование](../../contributing/testing/) или platform docs.
6. Если BuildTools, baking, scripting, application startup или embedding
   boundary могут затронуть минимальный проект, запустите соответствующий
   starter smoke.
7. Перегенерируйте затронутые contract models и выполните aggregate
   [generated contract diff](../../contributing/contract-change-management.md) для изменений
   project-facing API, CMake, CLI или package.
