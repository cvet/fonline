---
title: Сгенерированный реестр публичных репозиториев-примеров
document_id: generated-public-examples-index
locale: ru
generated: true
---

<!-- docs-translation: {"document_id":"generated-public-examples-index","locale":"ru","source_path":"Docs/en/reference/public-examples/index.md","source_sha256":"27fa05d5c1b58a9639e79e5e2a87c35f500dab2acb3347a66d411462060510f5"} -->

# Сгенерированный реестр публичных репозиториев-примеров

> Сгенерированный справочник. Не редактируйте эту страницу напрямую. Обновите `Examples/PublicRepositories.json` или управляющий overlay, затем выполните `python BuildTools/docs_examples.py --write`.

[Политика для разработчиков](../../how-to/build/public-example-repositories.md) | [Канонический JSON](../../../generated/public-examples.json) | [Исходный реестр](../../../../Examples/PublicRepositories.json)

Этот реестр описывает демонстрационные репозитории встраивающих проектов. Нормативное поведение движка определяется только исходным кодом Engine, тестами и документацией-владельцем.

## Контракт программы

| Поле | Значение |
| --- | --- |
| Организация | `cvet` |
| Репозиторий движка | `cvet/fonline` |
| Ревизия Engine для релиза | `exact-commit` |
| Ревизия для разработки | `master` (weekly) |
| Доставка обновлений | `reviewed-pull-request` |
| Digest контракта | `4af5ab4535823a5592d87d8e47c5403afd750a38697ffc6c62cf3065cba64e74` |

## Свидетельства публикации

Проверяйте вместе source status каждого репозитория, visibility/state remote, состояние наблюдённых required checks, точный Engine pin, политику update delivery и Contract digest. Свидетельством публикации является только source со статусом `published`, remote `public` / `published` и наблюдёнными checks `passing`. Строка private, reserved, source-staged, planned или not-observed остаётся предпубликационным свидетельством, даже если её исходники готовы.

## Текущее состояние реестра

- Source/remote: `4` source-ready, `4` private, and `0` published repositories.
- Наблюдённые состояния required checks: `not-observed`.
- Наблюдённые Engine pins: `project-template`=`9d74c751f5684f80aef3b35a0eb16a8fabf9fa42`, `minimal-multiplayer`=not observed, `content-showcase`=not observed, `native-extension-sample`=not observed.
- Значения программы, обязательные в том же отчёте: release Engine ref `exact-commit`, update delivery `reviewed-pull-request`, Contract digest `4af5ab4535823a5592d87d8e47c5403afd750a38697ffc6c62cf3065cba64e74`.
- `project-template`: source `source-ready`; remote `private` / `source-staged`; Engine pin `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42`; required checks `not-observed`.
- `minimal-multiplayer`: source `source-ready`; remote `private` / `reserved`; Engine pin not observed; required checks `not-observed`.
- `content-showcase`: source `source-ready`; remote `private` / `reserved`; Engine pin not observed; required checks `not-observed`.
- `native-extension-sample`: source `source-ready`; remote `private` / `reserved`; Engine pin not observed; required checks `not-observed`.

## Портфель

| Порядок | Репозиторий | Уровень | Статус исходников | Remote | Владелец | Назначение |
| ---: | --- | --- | --- | --- | --- | --- |
| 1 | `cvet/fonline-project-template` | `foundation` | `source-ready` | `private` / `source-staged` | Ответственные за сборку и релизы | Канонический шаблон GitHub и исходник быстрого старта до первого успешного headless-проекта. |
| 2 | `cvet/fonline-minimal-multiplayer` | `tutorial` | `source-ready` | `private` / `reserved` | Ответственные за документацию | Небольшой игровой вертикальный срез для первых руководств по серверу, клиенту, контенту, скриптам, персистентности и тестам. |
| 3 | `cvet/fonline-content-showcase` | `showcase` | `source-ready` | `private` / `reserved` | Ответственные за контент и ресурсы | Галерея презентационного качества для возможностей рендеринга и авторинга без большой кодовой базы игровой логики. |
| 4 | `cvet/fonline-native-extension-sample` | `advanced` | `source-ready` | `private` / `reserved` | Ответственные за runtime движка | Продвинутый минимальный пример проектной композиции C++, lifecycle-хуков, экспортов в скрипты, тестов и проверки ABI. |

## cvet/fonline-project-template

- Стабильный ID: `project-template`
- Исходники под ответственностью Engine: `Examples/MinimalProject`
- Remote: `private` / `source-staged` (created `2026-07-20`)
- Наблюдение remote: `2026-08-03`, branch `main`, head `9946ca42c332a294f8fedd2732e7850a01c1ec27`, required checks `not-observed`
- Зафиксированная ревизия Engine: `9d74c751f5684f80aef3b35a0eb16a8fabf9fa42`
- Зависимости: Нет
- Политика ресурсов: `none`

Возможности:

- `configure`
- `resource-bake`
- `headless-server`
- `native-extension`
- `remote-call-metadata`
- `deterministic-smoke`

Обязательные проверки:

- `governance-contract`
- `pinned-engine`
- `current-engine`
- `windows-smoke`
- `linux-smoke`

Критерий завершения: На чистом хосте Windows или Linux документированное успешное состояние сервера достигается менее чем за 30 минут из точной ревизии Engine, закреплённой тегом.

## cvet/fonline-minimal-multiplayer

- Стабильный ID: `minimal-multiplayer`
- Исходники под ответственностью Engine: `Examples/MinimalMultiplayer`
- Remote: `private` / `reserved` (created `2026-07-20`)
- Наблюдение remote: `2026-08-03`, branch `main`, head `97d232431488125b370be352fdcf28f66e6cbf4f`, required checks `not-observed`
- Зависимости: `project-template`
- Политика ресурсов: `project-original-or-permissive`

Возможности:

- `one-map-location`
- `player-and-npc`
- `item-interaction`
- `replicated-persisted-property`
- `event-and-remote-call`
- `english-and-russian-text`
- `client-visible-smoke`
- `manifest-driven-gameplay-smoke`
- `native-package-acceptance`
- `mapper-ui-capture`
- `spark-particle-authoring`

Обязательные проверки:

- `governance-contract`
- `pinned-engine`
- `current-engine`
- `windows-smoke`
- `linux-smoke`
- `windows-package`
- `linux-package`
- `tutorial-tag-replay`

Критерий завершения: Каждая контрольная точка руководства воспроизводится по закреплённому тегу, а итоговый проект понятен без Last Frontier или TLA.

## cvet/fonline-content-showcase

- Стабильный ID: `content-showcase`
- Исходники под ответственностью Engine: `Examples/ContentShowcase`
- Remote: `private` / `reserved` (created `2026-07-20`)
- Наблюдение remote: `2026-08-03`, branch `main`, head `011dab0d07eef6387609821206b8ee534ec51c3f`, required checks `not-observed`
- Зависимости: `project-template`, `minimal-multiplayer`
- Политика ресурсов: `audited-public-or-project-original`

Возможности:

- `sprites-and-animation`
- `lighting-and-effects`
- `particles-and-audio`
- `mapper-source-assets`
- `cross-backend-captures`
- `web-build`

Обязательные проверки:

- `governance-contract`
- `pinned-engine`
- `current-engine`
- `asset-provenance`
- `performance-budget`
- `web-build`
- `web-package`
- `web-runtime`
- `capture-reproduction`

Критерий завершения: Сборка по тегу выпускает публичную витрину и воспроизводимые снимки с полными машиночитаемыми сведениями о правах и происхождении ресурсов.

## cvet/fonline-native-extension-sample

- Стабильный ID: `native-extension-sample`
- Исходники под ответственностью Engine: `Examples/NativeExtensionSample`
- Remote: `private` / `reserved` (created `2026-07-20`)
- Наблюдение remote: `2026-08-03`, branch `main`, head `97823816ab333a62aced43edd4daafa19c5fee22`, required checks `not-observed`
- Зависимости: `project-template`
- Политика ресурсов: `none`

Возможности:

- `lifecycle-hook`
- `script-export`
- `role-specific-source`
- `focused-native-test`
- `abi-and-compatibility-review`

Обязательные проверки:

- `governance-contract`
- `pinned-engine`
- `current-engine`
- `native-unit-test`
- `native-extension-contract`

Критерий завершения: Пример по тегу демонстрирует один полный путь нативного расширения без игровых сервисов и проходит проверки сгенерированного контракта расширений.
