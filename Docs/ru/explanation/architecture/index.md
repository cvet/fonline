---
layout: default
title: Архитектура движка
locale: ru
document_id: engine-architecture
permalink: /Docs/ru/explanation/architecture/
---

<!-- docs-translation: {"document_id":"engine-architecture","locale":"ru","source_path":"Docs/en/explanation/architecture/index.md","source_sha256":"c029d979974afe0265f7b765970c086c3ce9c9f2355321fb74760dfc9ae2b6f4"} -->

# Архитектура движка

Этот документ содержит основанную на исходном коде карту слоёв FOnline. Используйте её, чтобы определить владельца поведения перед переходом к документации конкретной подсистемы.

## Решение о владении

Маршрутизируйте изменение через четыре решения:

1. Поведение, которое должно одинаково работать в нескольких играх, помещайте
   во владеющий слой исходников Engine и руководство соответствующей подсистемы.
   Правила игры, авторский контент, конфигурация продукта, пороги приёмки и
   release policy оставляйте во встраивающем проекте.
2. Используйте эту страницу архитектуры, когда поведение пересекает несколько
   слоёв Engine или границу Engine/project. Используйте [Дерево исходников](../../contributing/source-tree/),
   когда требуется узнать расположение кода или первый каталог для проверки.
3. Привязывайте генерируемые контракты Engine к владеющим исходникам Engine,
   машинной модели и generator. Входы проекта и генерируемые результаты проекта
   не становятся переиспользуемым авторитетом Engine только потому, что их
   читает или создаёт инструмент Engine.
4. Ссылайтесь с невладеющей страницы на владельца вместо повторения контракта.
   Перечень каталогов исходников не является архитектурным решением, а
   интеграция одного проекта не доказывает общее поведение Engine.

Полный ответ о границе называет и владельца, и маршрут документации:
используйте эту страницу для поведения уровня всей архитектуры, «Дерево
исходников» для навигации по коду, а руководство владеющей подсистемы для её
подробного контракта.

## Общая картина

FOnline состоит из переиспользуемого движка, встраиваемого игровым проектом. Игровой проект владеет контентом, скриптами, конфигурацией продукта и release policy; движок владеет переиспользуемыми runtime systems, tools, инфраструктурой generated API, platform frontends и композицией сборки.

<figure class="docs-diagram">
<picture>
<source media="(max-width: 700px)" srcset="../../../assets/diagrams/engine-game-boundary-mobile.svg">
<img src="../../../assets/diagrams/engine-game-boundary.svg" alt="Диаграмма показывает переиспользуемый движок FOnline слева и встраивающий игровой проект справа. Движок предоставляет runtime systems, tools, code generation и platform applications. Игра предоставляет project configuration, scripts, content, tests и release policy. Стороны соединяются generated contracts и extension hooks." loading="lazy">
</picture>
<figcaption>Движок владеет переиспользуемыми runtime и tooling; встраивающий проект владеет правилами игры, контентом, конфигурацией продукта, валидацией и release policy. Зависимости пересекают границу только через объявленную конфигурацию, generated contracts и extension hooks.</figcaption>
</figure>

Основные слои:

- **Applications** - точки входа исполняемых файлов и библиотек в `Source/Applications/`.
- **Essentials** - низкоуровневые platform, memory, filesystem, logging, serialization, sockets и utilities в `Source/Essentials/`.
- **Common runtime** - общая модель движка в `Source/Common/`: entities, properties, prototypes, maps, networking primitives, config, scripts и базовые services движка.
- **Client runtime** - presentation/resource/network-client сторона в `Source/Client/`.
- **Server runtime** - authoritative world, managers, database backends, network-server сторона и updater backend в `Source/Server/`.
- **Frontend** - абстракция application/window/rendering в `Source/Frontend/`.
- **Scripting** - AngelScript, Native, Mono и регистрация script methods в `Source/Scripting/`.
- **Tools** - baker, редактирование вокруг Mapper, asset processors и связанный developer tooling в `Source/Tools/`.
- **BuildTools** - CMake stages, helpers, toolchains, генерация platform projects, package layout и поддержка валидации в `BuildTools/`.

## Слой приложений

`Source/Applications/` является практическим каталогом точек входа. Он содержит обёртки приложений:

- `ClientApp.cpp` и `ClientLib.cpp` для client host/runtime flows.
- `ServerApp.cpp`, `ServerDaemonApp.cpp`, `ServerHeadlessApp.cpp` и `ServerServiceApp.cpp` для вариантов server.
- `MapperApp.cpp` для центрального интерактивного инструмента редактирования.
- `BakerApp.cpp` и `ASCompilerApp.cpp` для поддержки generation/build.
- `TestingApp.cpp` для выполнения тестов.

`BuildTools/cmake/stages/Applications.cmake` подключает эти файлы к project-specific targets в зависимости от build options, включая режимы client/server/tool/platform/library/headless. Не фиксируйте target names в документации движка: они часто выводятся из `FO_DEV_NAME` и presets встраивающего проекта.

Карта приложений приведена в [Applications](../../reference/applications.md).

## Проверенные пути исходного кода

- `Source/Applications/`
- `Source/Common/EngineBase.h`
- `Source/Common/EngineBase.cpp`
- `Source/Common/Entity.h`
- `Source/Common/Entity.cpp`
- `Source/Common/ScriptSystem.h`
- `Source/Common/ScriptSystem.cpp`
- `Source/Client/Client.h`
- `Source/Server/Server.h`
- `Source/Frontend/Application.h`
- `Source/Frontend/ApplicationInit.cpp`
- `BuildTools/cmake/stages/Applications.cmake`

## Слой общей среды выполнения

`Source/Common/` содержит общие понятия, используемые client, server, tools и scripts. Важные точки входа:

- `EngineBase.h` / `EngineBase.cpp` - базовые services движка и общее runtime state.
- `Entity.h` / `Entity.cpp` - экспортируемые entity concepts, общие для runtime sides.
- `Properties.h`, `EntityProperties.h`, `EntityProtos.h`, `ProtoManager.h` - модель property/prototype.
- `ScriptSystem.h` / `ScriptSystem.cpp` - абстракция script engine для runtime sides и tools.
- `Geometry.h`, `Movement.h`, `PathFinding.h`, `MapLoader.h` - переиспользуемые primitives карт и движения.
- `NetBuffer.h`, `NetworkUdp.h` - общие networking primitives.
- `ConfigFile.h`, `DataSource.h`, `FileSystem.h`, `CacheStorage.h` - поддержка config и data access.
- `ImageWriter.h` - кодировщики TGA/PNG для диагностических изображений, которые записывает движок: screenshots, captures render target и dumps atlas.

Этот слой должен оставаться переиспользуемым. Правила игры обычно выражаются через content/scripts или project-native extensions, а не через включение policy одного проекта в common engine code.

## Слои клиента и сервера

`Source/Client/Client.h` включает точки композиции client-side: интеграцию application, доступ к resource/cache, views для critters/items/locations/maps, effects, rendering-facing structures и client connection code.

`Source/Server/Server.h` включает authoritative runtime: entities, managers, database, geometry, scripting-facing server objects, client validation, networking и поддержку updater backend.

Документируйте client и server раздельно, потому что у них разные владельцы:

- **Client** представляет локальные views, resources, UI-facing objects и network-client behavior.
- **Server** владеет authoritative world state, persistence, entity managers, validation и network-server behavior.

## Слой frontend

`Source/Frontend/Application.h` и связанные файлы `Application*.cpp` / `Rendering*.cpp` абстрагируют запуск platform app и rendering backends. Различия headless/stub/native frontend принадлежат этому слою, а не документации игры.

Документация platform workflow:

- [Сборка, упаковка и отладка в браузере](../../how-to/platforms/web-debugging.md)
- [Сборка, упаковка и отладка на Android](../../how-to/platforms/android-debugging.md)
- [Нативная отладка и отладка AngelScript](../../troubleshooting/debugging.md)

## Слой scripting

`Source/Common/ScriptSystem.*` определяет общую абстракцию script system. `Source/Scripting/` предоставляет runtime-specific регистрацию methods и integration folders:

- `Source/Scripting/AngelScript/`
- `Source/Scripting/Native/`
- `Source/Scripting/Mono/`
- `Source/Scripting/*ScriptMethods.cpp`

Движок владеет переиспользуемым script/native bridge. Игровой проект владеет конкретными game script modules и gameplay logic.

## Слой сборки и генерации

`BuildTools/cmake/stages/` является staged CMake pipeline. Текущие stage files:

- `Init.cmake`
- `ProjectOptions.cmake`
- `CoreLibs.cmake`
- `ThirdParty.cmake`
- `EngineSources.cmake`
- `Codegen.cmake`
- `Applications.cmake`
- `ScriptsAndBaking.cmake`
- `Packages.cmake`
- `Finalize.cmake`

Эти stages компонуют code движка с конфигурацией встраивающего проекта. Перед изменением build behavior прочитайте [Процесс сборки](../../how-to/build/).

## Типичный runtime flow

Обычный workflow встраивающего проекта:

1. Repository игры конфигурирует CMake из корня проекта.
2. BuildTools загружает project options и engine sources.
3. Шаги codegen и baking подготавливают generated API/resources/scripts.
4. Applications собираются из точек входа `Source/Applications/`.
5. Runtime запускается через выбранное приложение: client, server, mapper, baker, test app или platform package.
6. Client/server/tools используют common runtime services и при необходимости вызывают принадлежащие игре scripts/content.

## Где документировать изменения

- Поведение уровня архитектуры: этот файл.
- Навигация по исходному коду: [Source Tree Guide](../../contributing/source-tree/).
- Точки входа приложений: [Applications](../../reference/applications.md).
- Workflow сборки: [процесс сборки](../../how-to/build/) и [конвейер BuildTools](../../reference/cmake-and-buildtools/pipeline.md).
- Граница script/native: [Nullability](../../../Nullability.md) и [Scripting](../../../Scripting.md).
- Отладка платформ: [Web](../../how-to/platforms/web-debugging.md), [Android](../../how-to/platforms/android-debugging.md) и [native debugging](../../troubleshooting/debugging.md).
