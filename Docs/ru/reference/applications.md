---
layout: default
title: Приложения и точки входа
locale: ru
document_id: applications-entry-points
permalink: /Docs/ru/reference/applications.html
---

<!-- docs-translation: {"document_id":"applications-entry-points","locale":"ru","source_path":"Docs/en/reference/applications.md","source_sha256":"d09f450e25e6551cad72f2c374de387b4619d00d538e786c023743307803e59d"} -->

# Приложения и точки входа

Этот документ сопоставляет точки входа приложений в `Source/Applications/` и их wiring в BuildTools.

## Владение build target

Исходные файлы приложений принадлежат движку, но target names и включение часто зависят от проекта. `BuildTools/cmake/stages/Applications.cmake` создаёт targets из этих точек входа с помощью project variables, включая `FO_DEV_NAME`, platform options и feature toggles режимов client/server/library/headless.

Поэтому документ называет source entrypoints и роли, а не универсальные конечные target names.

## Проверенные пути исходного кода

- `Source/Applications/ASCompilerApp.cpp`
- `Source/Applications/AnimationViewerApp.cpp`
- `Source/Applications/BakerApp.cpp`
- `Source/Applications/BakerLib.cpp`
- `Source/Applications/ClientApp.cpp`
- `Source/Applications/ClientLib.cpp`
- `Source/Applications/MapperApp.cpp`
- `Source/Applications/ParticleViewerApp.cpp`
- `Source/Applications/ServerApp.cpp`
- `Source/Applications/ServerDaemonApp.cpp`
- `Source/Applications/ServerHeadlessApp.cpp`
- `Source/Applications/ServerServiceApp.cpp`
- `Source/Applications/TestingApp.cpp`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/helpers/Build.cmake`

## Файлы точек входа

- `Source/Applications/ClientApp.cpp` - точка входа host исполняемого client.
- `Source/Applications/ClientLib.cpp` - точка входа client runtime library для workflow с разделением host/runtime.
- `Source/Applications/ServerApp.cpp` - точка входа стандартного server application.
- `Source/Applications/ServerDaemonApp.cpp` - вариант server в стиле daemon.
- `Source/Applications/ServerHeadlessApp.cpp` - вариант headless server.
- `Source/Applications/ServerServiceApp.cpp` - вариант server в стиле service.
- `Source/Applications/MapperApp.cpp` - точка входа инструмента Mapper.
- `Source/Applications/AnimationViewerApp.cpp` - standalone host preview critter animation.
- `Source/Applications/ParticleViewerApp.cpp` - standalone host preview baked particles.
- `Source/Applications/BakerApp.cpp` - точка входа baking build/resources.
- `Source/Applications/BakerLib.cpp` - точка входа baking library при композиции baking как library.
- `Source/Applications/ASCompilerApp.cpp` - точка входа AngelScript compiler.
- `Source/Applications/TestingApp.cpp` - точка входа приложения test runner.

## Wiring CMake

`BuildTools/cmake/stages/Applications.cmake` является главным источником создания application targets. Он использует helpers `AddExecutableApplication` и `AddSharedApplication` для executable или shared-library outputs и назначения platform-specific properties.

Наблюдаемые patterns wiring:

- Client executable и client runtime library создаются, когда включены client builds.
- Headless variants используют headless frontend libraries, где это применимо.
- Варианты server выбираются server/platform/service options.
- Mapper, AnimationViewer и ParticleViewer создаются вместе при включённом `FO_BUILD_MAPPER`. Viewers переиспользуют client и baker services, но не запускают Mapper или networked client loop.
- Test applications помечаются как testing apps для отдельной обработки от product runtime apps.

Перед объявлением target доступным прочитайте CMake stage. Доступность может зависеть от platform и project options.

## Hook настроек запуска клиента

Приложения, создающие `ClientEngine`, вызывают предоставленный проектом `ClientStartupSettingsHook(GlobalSettings&, int32_t clientIndex, bool embedded)` непосредственно перед созданием client:

- `ClientApp.cpp` вызывает его для standalone client с `clientIndex = 1` и `embedded = false`.
- `ServerApp.cpp` вызывает его для каждого GUI embedded client после копирования app settings в принадлежащий client `GlobalSettings`.
- `ServerHeadlessApp.cpp` вызывает его для каждого headless embedded client после копирования app settings в принадлежащий client `GlobalSettings`.

Встроенные в server clients сохраняют отдельные settings objects на всё время жизни своего `ClientEngine`. Это позволяет встраивающему проекту менять per-client settings, включая auth identity, AI-control ports, diagnostics или transport toggles, не изменяя server `App->Settings` или уже созданные clients. Hook предназначен только для startup-time configuration; после создания `ClientEngine` продолжают действовать обычные client/server authority и правила script-visible settings.

Standalone animation и particle viewers также создают `ClientEngine`, но намеренно используют application settings напрямую и не вызывают gameplay startup hook: у них нет account, network session или embedded client index.

## Какую точку входа проверять?

- Client startup или host/runtime behavior: `ClientApp.cpp`, `ClientLib.cpp`, [Client Updater](../explanation/runtime/client-updater.md).
- Жизненный цикл сервера: `ServerApp.cpp` и серверные варианты, [Серверная среда выполнения](../explanation/runtime/server.md).
- Resource generation: `BakerApp.cpp`, `BakerLib.cpp`, [Baking Pipeline](../explanation/content-pipeline/baking.md).
- Script compilation: `ASCompilerApp.cpp`, [Scripting](../../Scripting.md) и [Generated API and Metadata](metadata/index.md).
- Автоматизация Mapper: `MapperApp.cpp`, [инструменты Mapper](../how-to/tools/mapper.md).
- Animation inspection: `AnimationViewerApp.cpp`, `Source/Tools/AnimationViewer.*`, [просмотр анимации и частиц](../how-to/tools/animation-particle-viewers.md).
- Particle inspection: `ParticleViewerApp.cpp`, `Source/Tools/ParticleViewer.*`, [просмотр анимации и частиц](../how-to/tools/animation-particle-viewers.md) и [Particle Format](../how-to/content/particle-format.md).
- Tests: `TestingApp.cpp`, [Source/Tests README](../../../Source/Tests/README.ru.md) и [Testing](../../Testing.md).

## Правило документации

При добавлении или изменении application entrypoint:

1. Обновите `BuildTools/cmake/stages/Applications.cmake` или его helpers, если меняется создание target.
2. Обновите этот файл ролью entrypoint.
3. Обновите тематический subsystem doc, если меняется runtime behavior.
4. Обновите `BuildTools/PackageInterface.json`, если меняются package roles или compatibility.
5. Проверяйте через соответствующий preset/target встраивающего проекта, а не предполагаемый универсальный target name.
