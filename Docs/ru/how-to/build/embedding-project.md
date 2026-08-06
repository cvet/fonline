---
layout: default
title: Встраивание FOnline в игровой проект
locale: ru
document_id: embedding-project
permalink: /Docs/ru/how-to/build/embedding-project.html
---

# Встраивание FOnline в игровой проект

<!-- docs-translation: {"document_id":"embedding-project","locale":"ru","source_path":"Docs/en/how-to/build/embedding-project.md","source_sha256":"ce02151c8179929c91e596e69f3e4e02042a60e4fb0e3939bdc5b3fa9394a270"} -->

FOnline рассчитан на подключение как source submodule. Репозиторий движка
поставляет переиспользуемую технологию, а репозиторий игры создает конкретный
продукт.

## Проверенные исходные пути

- `BuildTools/Init.cmake`
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `Examples/MinimalProject/CMakeLists.txt`
- `Examples/MinimalProject/FOnlineStarter.fomain`
- `Examples/MinimalProject/README.md`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/FOnlineMinimalMultiplayer.fomain`
- `Examples/PublicRepositories.json`
- `Source/Applications`
- `Source/Tools`

Принадлежащий движку
[минимальный проект](../../../../Examples/MinimalProject/README.ru.md) является
каноническим исполняемым примером этой границы. Он также служит источником для
[руководства по первому headless-проекту](../../tutorials/first-project.md) и
планируемого `fonline-project-template`. Ownership, exact-revision rules, CI
lanes и publication gates этого и последующих репозиториев определены в
[публичных репозиториях с примерами](public-example-repositories.md).

## Ожидаемая структура репозитория

Типичный игровой репозиторий выглядит так:

```text
GameProject/
├── Engine/                 # git submodule pointing to this repository
├── CMakeLists.txt          # project entry point that includes engine build logic
├── CMakePresets.json       # project presets and platform variants
├── GameName.fomain         # master project configuration
├── Scripts/                # game AngelScript modules
├── SourceExt/              # optional project-native C++ extensions
├── Critters/ Items/ Maps/  # game content and prototypes
├── ProjectDialogs/ Texts/  # optional project-defined dialogs and localization
└── Docs/                   # game-specific documentation
```

Имена каталогов могут различаться, но ownership rule должен оставаться
стабильным: переиспользуемая механика движка находится в `Engine/`, а контент
игры и проектная политика принадлежат родительскому репозиторию.

Приведенное имя каталога намеренно обобщено. Сейчас FOnline не поставляет
встроенную dialog-tree schema, `.fodlg` parser, dialog baker, runtime или visual
editor. Игра может реализовать dialogs в scripts, через project-native
extensions и bakers либо через отдельно версионируемый companion. Документируйте
выбранный format и validation в игровом репозитории и не предполагайте, что
другой проект использует те же dialog API или layout файлов.

## Что принадлежит движку

В репозитории движка должны находиться:

- runtime systems, общие для нескольких игр;
- BuildTools и CMake stages для композиции проектов;
- генерация platform packages/workspaces;
- ресурсы движка и reusable tools;
- определения public/native API и механика generated scripting API;
- документация о поведении движка, platform mechanics и reusable contracts.

## Что принадлежит игровому проекту

В игровом проекте должны находиться:

- правила игры, content, maps, prototypes, dialogs, localization и GUI definitions;
- game-specific AngelScript modules;
- project-level native extension implementations и dependencies;
  [Native Extensions](../../../NativeExtensions.md) определяет composition, hooks
  и bindings, а [Project Dependencies](../../../ProjectDependencies.md) владеет
  выбором library/SDK, role-scoped linking, package delivery и updates;
- project-level AI observations, game actions, MCP tools и listener shipping
  policy; [протокол AiControl](../ai-control-protocol.md) владеет только
  reusable transport, command lifecycle, threat boundary, reference client и
  protocol evidence;
- project presets, product identifiers, package names, signing/deployment
  choices и CI policy;
- game design и content workflow documentation.

## Проектные форматы игровых систем

Проект может определять authored formats для игровых систем, не входящих в
контракт движка. Типичный пример: dialog trees. Система остается project-owned,
пока ее reusable implementation, tests, fixtures, compatibility policy и
документация не перенесены в Engine или versioned companion.

Полный project-owned format должен определять:

1. parser и authoritative grammar;
2. baker или другие generated outputs;
3. runtime consumers и authority boundaries;
4. editor/formatter behavior и round-trip expectations;
5. source-level, compiled и runtime validation;
6. compatibility и migration policy между Engine pins;
7. точную ownership label в документации проекта.

Документация движка может описывать native-extension и baking primitives,
использованные для реализации format. Она не должна представлять проектный
format как стандартную возможность FOnline.

## Композиция сборки

Сборкой должен управлять игровой репозиторий. На практике:

1. Выполняйте configure из корня игрового репозитория, а не из `Engine/`, если
   конкретный engine-only workflow не требует иного.
2. Используйте `CMakePresets.json` и tasks игрового проекта, чтобы generated
   paths, target names и package metadata соответствовали продукту.
3. Используйте engine `BuildTools` как поставщика reusable stages и helpers.
4. Не включайте generated files в hand-authored docs, если generation process
   не является предметом страницы.

Используйте `Examples/MinimalProject/CMakeLists.txt` как минимальный актуальный
пример композиции. Он также доказывает server-only `INTERFACE` dependency через
`AddProjectLibraries`; расширяйте его project-owned modules и targets, не
копируя постороннее wiring из большой игры.

### Добавление проектной цели запекания

Стандартный конвейер создаёт `BakeResources` и `ForceBakeResources` с subconfig
`NONE`. Если игра определяет отдельный срез конфигурации для публичного,
тестового или релизного набора ресурсов, создайте его цель сразу после стадии
scripts-and-baking:

```cmake
SetupScriptsAndBaking()

AddBakingTarget(Game_PublicResources
    SUB_CONFIG PublicGame
    COMMENT "Bake public resources")

BuildPackages()
```

Добавляйте `FORCE`, только если эта цель всегда должна запрашивать полное
запекание. Helper сохраняет стандартные зависимость от `ForceCodeGeneration`,
рабочий каталог `FO_OUTPUT_PATH`, аргумент главной конфигурации и обновление
resource build hash. Имя цели, содержимое subconfig и последующая политика
CI/пакетов должны оставаться в репозитории игры.

## Композиция документации

Используйте следующую маршрутизацию:

- из game docs ссылайтесь на `Engine/Docs/...` для reusable mechanics, например
  Web/Android debugging, nullability, updater protocol, mapper automation и
  native debugging;
- локальные ссылки в engine docs должны оставаться внутри репозитория движка;
  cross-project examples используют стабильные HTTPS links на tagged public
  revisions;
- используйте только примеры, generated registry status которых равен
  `published`; планируемое имя репозитория не является публичным source link;
- не дублируйте длинные объяснения движка в game docs: оставьте короткую
  project-specific note и ссылку на владеющий engine document.

## Принцип проверки

По возможности проверяйте изменения движка через реальный embedding project.
Минимальный проект движка дает baseline-маршруты `win64-starter-smoke` и
`linux-starter-smoke`; для client, content, packaging и gameplay contracts все
еще нужны более крупные проекты. Reusable engine change может компилироваться
изолированно, но ломать generated API, project packaging, scripts или content
baking. Выбирайте самый узкий project target, который использует измененный
слой.
