---
layout: default
title: FOnline Engine
locale: ru
document_id: repository-home
permalink: /README.ru.html
---

<!-- docs-translation: {"document_id":"repository-home","locale":"ru","source_path":"README.md","source_sha256":"044cdb1c36e2090cd24300e92927cb11a1cb1462e51ead9520af17df1d6a7535"} -->

# FOnline Engine

[![License](https://img.shields.io/github/license/cvet/fonline.svg)](https://github.com/cvet/fonline/blob/master/LICENSE)
[![GitHub](https://github.com/cvet/fonline/workflows/validate/badge.svg)](https://github.com/cvet/fonline/actions)
[![Commit](https://img.shields.io/github/last-commit/cvet/fonline.svg)](https://github.com/cvet/fonline/commits/master)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/cvet/fonline)

**FOnline** — открытый движок на C++20 под лицензией MIT для создания сетевых
многопользовательских RPG в классическом изометрическом стиле Fallout
1/2/Tactics и Arcanum. Единая кодовая база предоставляет авторитетный сервер,
игровой клиент, редактор карт, конвейер контента и упаковку для настольных,
мобильных и браузерных платформ. Сама игра остаётся за вами: контент, скрипты и
правила находятся в отдельном репозитории, который встраивает движок.

Движок непрерывно развивается с 2006 года и используется многопользовательскими
RPG сообщества. Актуальный пример — [Last Frontier](https://lastfrontier.ru/),
постапокалиптическая MMO на его основе.

## Почему FOnline?

- **Многопользовательская архитектура прежде всего.** Это не одиночный движок с
  добавленной впоследствии сетью: авторитетный сервер, реплицируемое состояние
  сущностей и разделение клиента и сервера заложены в основу модели сущностей.
- **Полный вертикальный стек.** Сервер, клиент, mapper, редактор, resource baker,
  компилятор скриптов, runner тестов и автообновление собираются из одних
  исходников единым конвейером CMake.
- **Чистое разделение движка и игры.** Движок остаётся переиспользуемым
  подмодулем; игра владеет контентом, скриптами, конфигурацией, брендом и
  политикой выпуска. Обновления движка не переносят в игру его продуктовую
  политику.
- **Контент на основе данных.** Прототипы, карты, диалоги, локализация и GUI
  создаются как текстовые ресурсы и запекаются в runtime-пакеты, удобные для
  diff, review и инструментов.
- **Работает там, где находятся игроки.** Нативные Windows, Linux и macOS,
  Android и iOS, а также WebAssembly-клиент, работающий в браузере через
  WebSocket.

## Основные возможности

### Многопользовательское ядро

- Авторитетная серверная среда с менеджерами сущностей, проверкой клиента и
  защищённым разбором недоверенного клиентского ввода.
- Общая модель сущностей, свойств и прототипов со сгенерированными
  типобезопасными wrapper свойств и автоматической репликацией клиентам.
- Сменные сетевые транспорты: TCP-сокеты, включая сервер на Asio, WebSocket для
  браузерной игры, упорядоченный UDP-канал и внутрипроцессный транспорт для
  тестов и встроенных клиентов.
- Сменные backend постоянного хранения — JSON-файлы, SQLite, MongoDB или память
  — за единым фасадом базы данных с асинхронной очередью commit и журналами
  восстановления.
- Встроенное автообновление клиента: тонкий клиентский host, заменяемая runtime,
  возобновляемая передача файлов и серверный backend обновления.

### Скрипты

- Игровые скрипты AngelScript поверх независимой от backend скриптовой системы.
- Нативный API экспортируется в скрипты генератором кода из аннотаций `///@`:
  методы, свойства, события, remote call и enum автоматически остаются
  согласованными с исходниками C++.
- Nullability контролируется на границе скриптов и нативного кода: скриптовый
  `T?` соответствует нативным контрактам `ptr<T>`/`nptr<T>`, которые проверяются
  анализаторами и runtime assert.
- Отладка скриптов рядом с нативной отладкой.

### Отрисовка и представление

- Backend отрисовки: OpenGL, Direct3D, Vulkan и SDL_GPU, а также headless/null
  режимы для серверов и CI.
- Эффекты пишутся один раз на GLSL, компилируются через glslang в SPIR-V и
  переводятся для каждого backend с помощью SPIRV-Cross.
- Изометрические миры на основе спрайтов, 3D-модели персонажей FBX, частицы,
  видео и звук в современных Ogg/Vorbis и классических форматах Fallout.
- Оконный, безрамочный полноэкранный и виртуальный многооконный режимы клиента с
  общей моделью разрешения и letterbox; инструменты разработчика на ImGui.

### Мир и карты

- Геометрия с шестиугольной и квадратной сеткой и общими функциями расстояния,
  направления и окрестности.
- Поиск пути, трассировка линий, контексты движения и модель блокировки,
  рассчитанные на авторитетный многопользовательский сервер.

### Конвейер контента и инструменты

- Конвейер запекания превращает авторские прототипы, карты, диалоги,
  локализованные тексты, эффекты, изображения, модели и скрипты в версионированные
  пакеты runtime-ресурсов.
- Импорт классических 2D-форматов Fallout FRM, Arcanum ART и других устаревших
  форматов рядом с PNG/TGA.
- Интерактивные инструменты на самом движке: Mapper с окнами карт и контента и
  редактированием SPARK, а также отдельные viewer анимаций и запечённых частиц.

### Инженерное качество

- Unit-тесты Catch2 со сгенерированными целями отдельных suite, запусками
  sanitizer и покрытием кода.
- Clang Thread Safety Analysis применяется как `-Werror` на каждом Clang
  toolchain; строгие словари smart pointer и nullability проверяются во всей
  кодовой базе.
- Постоянные stack trace, детерминированные правила exception safety и модель
  terminate-on-OOM вместо частично изменённого состояния.
- Интеграция Tracy profiler для capture клиента и сервера.

## Архитектура в одном взгляде

```text
Your game repository                      FOnline engine (this repo, embedded as Engine/)
────────────────────                      ────────────────────────────────────────────────
content: protos, maps,            ┌──►    Applications — client/server/tool entry points
dialogs, texts, GUI               │       Client & Server runtimes — views vs. authority
AngelScript game logic     embeds │       Common model — entities, properties, protos,
.fomain configuration      ───────┤                      maps, networking, config
native extensions                 │       Frontend — windows, input, audio, renderers
CMake presets, CI,                │       Scripting — AngelScript bridge + generated API
release policy                    └──►    Tools & BuildTools — bakers, mapper, editor,
                                                     CMake stages, codegen, packaging
```

Движок владеет переиспользуемой технологией, а игра — продуктом. Игровой
репозиторий добавляет движок как подмодуль `Engine/`, направляет его стадийный
конвейер CMake на собственную конфигурацию и получает именованные проектом цели
сборки для каждого приложения. Полная карта слоёв находится в
[Архитектура движка](Docs/ru/explanation/architecture/), а контракт встраивания — в
[Встраивание FOnline в игровой проект](Docs/ru/how-to/build/embedding-project.md):

```text
GameProject/
├── Engine/                 # this repository as a git submodule
├── CMakeLists.txt          # project entry point that includes engine build logic
├── CMakePresets.json       # project presets and platform variants
├── GameName.fomain         # master project configuration
├── Scripts/                # game AngelScript modules
├── SourceExt/              # optional project-native C++ extensions
├── Critters/ Items/ Maps/  # game content and prototypes
└── Dialogs/ Texts/         # dialogs and localization
```

## Начало работы

- **Запустите первый проект движка:** [Первый headless-проект FOnline](Docs/ru/tutorials/first-project.md) — сконфигурируйте, соберите, запеките, запустите, проверьте и остановите минимальный headless-сервер.
- **Изучите канонический scaffold:** [Examples/MinimalProject/README.ru.md](Examples/MinimalProject/README.ru.md) — полный проект и контракт smoke-проверки CI.
- **Изучите исполняемую галерею контента:** [Examples/ContentShowcase/README.ru.md](Examples/ContentShowcase/README.ru.md) — исходные материалы, baking, native/Web-проверки, происхождение, бюджеты и воспроизводимые доказательства захвата.
- **Соберите первый игровой срез:** [первый игровой клиент](Docs/ru/tutorials/first-client.md), [первое изменение контента](Docs/ru/tutorials/first-content.md) и [первый автоматизированный тест](Docs/ru/tutorials/first-test.md) — подключите клиент, измените локализованный контент и расширьте исполняемые проверки.
- **Настройте и сопровождайте проект:** [Конфигурация игрового проекта](Docs/ru/how-to/build/project-configuration.md), [Generated Content Workflow](Docs/ru/how-to/build/generated-content.md) и [руководство по обновлению Engine](Docs/ru/how-to/migration/engine-upgrade.md) — владение `.fomain`, пакетами ресурсов, сгенерированными результатами, миграциями и обновлениями Engine.
- **Доказательно выбирайте release-цели:** [матрица поддержки](Docs/ru/reference/platforms/support-matrix.md) и [сгенерированная матрица](Docs/ru/reference/platforms/generated-matrix.md) — различайте возможности исходников, обязательные сборки, process smoke и квалификацию проекта или устройства.
- **Планируйте и проверяйте публичные примеры:** [Публичные репозитории с примерами](Docs/ru/how-to/build/public-example-repositories.md) и [сгенерированный реестр](Docs/ru/reference/public-examples/index.md) — владение, точные Engine pin, compatibility lane, шаблон репозитория, поддержка и происхождение ресурсов.
- **Просматривайте сгенерированный нативный API:** [Docs/ru/reference/script-api/index.md](Docs/ru/reference/script-api/index.md) — методы, свойства, события, типы, настройки, миграции и ссылки на исходники.
- **Создавайте прототипы:** [Формат прототипов](Docs/ru/how-to/content/prototype-format.md) и [сгенерированный справочник](Docs/ru/reference/prototype-format/index.md) — точный синтаксис, наследование, встроенные свойства, ссылки, миграции и проверка.
- **Создавайте карты:** [Docs/ru/how-to/content/map-format.md](Docs/ru/how-to/content/map-format.md) и [сгенерированный справочник](Docs/ru/reference/map-format/index.md) — секции `.fomap`, placement ID, владение, нормализация Mapper, запекание по сторонам и загрузка во время выполнения.
- **Создавайте локализованный текст:** [Текст и локализация](Docs/ru/how-to/content/text-and-localization.md) и [сгенерированный справочник](Docs/ru/reference/text-format/index.md) — синтаксис `.fotxt`, нормализация языков, `$Text` прототипов, runtime lookup, цветовые теги и граница игрового форматирования.
- **Создавайте изображения и sprite sheet:** [форматы изображений и спрайтов](Docs/ru/how-to/content/image-format.md) и [сгенерированный справочник](Docs/ru/reference/image-format/index.md) — PNG/TGA и устаревший импорт, композиция FOFRM, запекание, runtime factory, атласы, cache и проверка.
- **Создавайте shader-эффекты:** [Effect Format](Docs/ru/how-to/content/effect-format.md) и [сгенерированный справочник](Docs/ru/reference/effect-format/index.md) — секции `.fofx`, проходы, render state, shader resources, результаты backend, runtime selection и скриптовые значения.
- **Создавайте частицы:** [Формат и исполнение частиц](Docs/ru/how-to/content/particle-format.md) и [сгенерированный справочник](Docs/ru/reference/particle-format/index.md) — выбор SPARK/Effekseer, авторинг `.spark`/`.efkproj`, запекание `.spk`/`.efk`, инструменты Mapper, runtime route и интеграция с моделями и скриптами.
- **Создавайте bitmap-шрифты и раскладывайте текст:** [форматы шрифтов и компоновка текста](Docs/ru/how-to/content/font-format.md) и [сгенерированный справочник](Docs/ru/reference/font-format/index.md) - дескрипторы FOFNT/BMFont, привязка slot, масштабирование, измерение, перенос, флаги отрисовки, цвета и проверка.
- **Изучайте контракты интеграции проекта:** [справочник CMake](Docs/ru/reference/cmake/index.md), [справочник BuildTools CLI](Docs/ru/reference/buildtools/index.md), [справочник helper CLI](Docs/ru/reference/helper-cli/index.md), [справочник нативных расширений](Docs/ru/reference/native-extension/index.md), [справочник пакетов](Docs/ru/reference/packages/index.md) и [реестр публичных примеров](Docs/ru/reference/public-examples/index.md) — точные поверхности CMake, основного и вспомогательного BuildTools, нативных расширений, упаковки и программы примеров.
- **Безопасно добавляйте нативный C++ проекта:** [Нативные расширения](Docs/ru/how-to/native-extensions.md) — роли исходников, хуки, экспорт в скрипты, владение состоянием, совместимость и исполняемая проверка.
- **Просматривайте сгенерированный интерфейс CMake:** [Docs/ru/reference/cmake/index.md](Docs/ru/reference/cmake/index.md) — опции проекта, строгие стадии и hook, выбранные helper, значения по умолчанию и ссылки на исходники.
- **Измеряйте производительность клиента и сервера:** [Профилирование](Docs/ru/how-to/quality/profiling.md) — режимы сборки Tracy, изолированные границы capture, воспроизводимые нагрузки и сопоставимый анализ результатов.
- **Публикуйте документацию:** [руководство по публикации сайта](Docs/ru/contributing/documentation/site-publication.md) — сгенерированные navigation/search/route, rolling version и locale policy, локальный Jekyll preview, проверка rendered route и доступности, артефакты CI и существующий GitHub Pages маршрут `fonline.ru`.
- **Используйте документацию для AI и offline:** [llms.txt](llms.txt), [llms-full.txt](llms-full.txt) и [docs-manifest.json](docs-manifest.json) — сгенерированные маршруты, ограниченный контекст, canonical/source URL, provenance и хэши контента из того же Markdown-манифеста.
- **Если вы впервые знакомитесь с движком:** [Начало работы](Docs/ru/tutorials/getting-started.md) — первый маршрут: что читать, что собирать и кому что принадлежит.
- **Если вы начинаете или изучаете игровой проект:** [Встраивание FOnline в игровой проект](Docs/ru/how-to/build/embedding-project.md) — ожидаемая структура репозитория и правила владения.
- **Если вы собираете проект:** [Процесс сборки](Docs/ru/how-to/build/) — предварительные требования, пресеты и стратегия проверки. Обычно сборка запускается из встраивающего игрового репозитория, а не из checkout движка.
- **Инструкции для AI-сопровождения:** [AGENTS.md](AGENTS.md) — прочитайте перед изменением кода или документации движка.

FOnline имеет профили сборки для Windows, Linux, macOS, Android, iOS и
WebAssembly, но поддержка ограничена доказательствами. Сверяйтесь с
[матрицей поддержки](Docs/ru/reference/platforms/support-matrix.md): cross-build или headless smoke не
квалифицирует renderer, устройство, пакет, сервис или маршрут магазина.

## Структура репозитория

- [`Source/`](Source/) — исходники движка: точки входа `Applications/`, runtime
  клиента и сервера, общая модель `Common/`, платформенный и render-слой
  `Frontend/`, bridge скриптов, baker и редакторы `Tools/`, низкоуровневое ядро
  `Essentials/`, unit-тесты `Tests/`.
- [`BuildTools/`](BuildTools/) — стадийный конвейер CMake, генерация кода,
  платформенные toolchain, подготовка workspace и пакетов.
- [`Resources/`](Resources/) — принадлежащие движку runtime и build resources.
- [`ThirdParty/`](ThirdParty/) — поставляемые зависимости SDL, AngelScript,
  Asio, ImGui, glslang, SPIRV-Cross, Tracy и другие; процесс сопровождения
  описан в разделе [Сопровождение ThirdParty](Docs/ru/contributing/third-party/).
- [`Docs/`](Docs/) — сопровождаемая документация движка.

## Документация

Сопровождаемый индекс — [Docs/ru/index.md](Docs/ru/index.md). Подробные руководства
по темам:

| Тема | Документы |
|-------|-----------|
| Архитектура и навигация | [Архитектура](Docs/ru/explanation/architecture/) · [Дерево исходного кода](Docs/ru/contributing/source-tree/) · [Приложения](Docs/ru/reference/applications.md) · [Essentials](Docs/ru/reference/native/essentials.md) |
| Модель среды выполнения | [Модель сущностей](Docs/ru/explanation/entity-and-property-model/) · [Карты и движение](Docs/ru/explanation/maps-and-movement.md) · [Сеть](Docs/ru/explanation/authority-and-networking/) · [Сохранение данных](Docs/ru/explanation/persistence/) |
| Клиент и сервер | [Клиентская среда выполнения](Docs/ru/explanation/runtime/client.md) · [Серверная среда выполнения](Docs/ru/explanation/runtime/server.md) · [Frontend и рендеринг](Docs/ru/explanation/rendering/) · [Client Updater](Docs/ru/explanation/runtime/client-updater.md) |
| Скрипты | [Scripting](Docs/ru/explanation/scripting-runtime/) · [LifecycleAndConcurrency](Docs/ru/how-to/scripting/lifecycle-and-concurrency.md) · [RemoteCalls](Docs/ru/reference/scripting/remote-calls.md) · [ScriptMethodsMap](Docs/ru/reference/script-api/method-ownership.md) · [Nullability](Docs/ru/contributing/coding-contracts/nullability.md) · [GeneratedApiAndMetadata](Docs/ru/reference/metadata/index.md) · [Управление изменениями контрактов](Docs/ru/contributing/contract-change-management.md) |
| Сборка и конвейер контента | [BuildWorkflow](Docs/ru/how-to/build/) · [ProjectConfiguration](Docs/ru/how-to/build/project-configuration.md) · [GeneratedContentWorkflow](Docs/ru/how-to/build/generated-content.md) · [EngineUpgradeGuide](Docs/ru/how-to/migration/engine-upgrade.md) · [SupportMatrix](Docs/ru/reference/platforms/support-matrix.md) · [BuildToolsPipeline](Docs/ru/reference/cmake-and-buildtools/pipeline.md) · [BakingPipeline](Docs/ru/explanation/content-pipeline/baking.md) · [ConfigurationAndDataSources](Docs/ru/reference/settings/configuration-and-data-sources.md) |
| Инструменты | [Tools](Docs/ru/reference/tools/) · [Инструменты Mapper](Docs/ru/how-to/tools/mapper.md) · [Интерактивное руководство по Mapper](Docs/ru/how-to/tools/mapper-interactive.md) · [Просмотр анимации и частиц](Docs/ru/how-to/tools/animation-particle-viewers.md) |
| Качество и соглашения | [Тестирование](Docs/ru/contributing/testing/index.md) · [Профилирование](Docs/ru/how-to/quality/profiling.md) · [ExceptionSafety](Docs/ru/contributing/coding-contracts/exception-safety.md) · [SmartPointers](Docs/ru/contributing/coding-contracts/smart-pointers.md) · [ThreadSafetyAnalysis](Docs/ru/contributing/coding-contracts/thread-safety-analysis.md) |
| Отладка платформ | [Debugging](Docs/ru/troubleshooting/debugging.md) · [Web](Docs/ru/how-to/platforms/web-debugging.md) · [Android](Docs/ru/how-to/platforms/android-debugging.md) |

Когда поведение заметно меняется, документ-владелец обновляется в том же
изменении. Документация сопровождается как основанный на исходниках справочник,
а не как запоздалое дополнение.

## Проект и сообщество

- Сайт: <https://fonline.ru>
- GitHub: <https://github.com/cvet/fonline>
- Лицензия: [MIT](LICENSE)
