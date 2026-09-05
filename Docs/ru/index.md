---
layout: default
title: Документация движка FOnline
locale: ru
document_id: documentation-home
permalink: /Docs/ru/
---

<!-- docs-translation: {"document_id":"documentation-home","locale":"ru","source_path":"Docs/en/index.md","source_sha256":"fc22a1f7d15f57e32c438fb7e36cb65d284f401c469c4ae422f8a29d23df937d"} -->

# Документация движка FOnline

Это главная страница пользовательской документации переиспользуемого движка
FOnline. Она предназначена для разработчиков игр, авторов инструментов,
релиз-инженеров и разработчиков движка, работающих с отдельной копией Engine
без документации другого игрового проекта.

## С чего начать

- [Начало работы](tutorials/getting-started.md) знакомит нового разработчика с
  репозиторием, поддерживаемыми процессами и границей ответственности движка и
  проекта.
- [Первый headless-проект FOnline](tutorials/first-project.md) настраивает,
  собирает и запускает минимальный проверенный сервер.
- [Первый игровой клиент](tutorials/first-client.md) добавляет подключённый
  настольный клиент, загрузку карты и управляемое сервером взаимодействие.
- [Первое изменение контента](tutorials/first-content.md) проводит
  локализованные данные прототипа через запекание и runtime-поиск.
- [Первый автоматизированный тест](tutorials/first-test.md) добавляет проверки
  метаданных, серверного контента и результата, видимого клиенту.
- [Минимальный проект](../../Examples/MinimalProject/README.ru.md) и
  [минимальный многопользовательский проект](../../Examples/MinimalMultiplayer/README.ru.md)
  являются принадлежащими движку исполняемыми исходниками учебных материалов.

## Сборка и выпуск игры

- [Подключение Engine к проекту](how-to/build/embedding-project.md) определяет
  границу репозитория между игрой и закреплённой ревизией движка.
- [Конфигурация проекта](how-to/build/project-configuration.md) описывает
  `.fomain`, пакеты ресурсов, sub-config, переопределения и проверку.
- [Процесс сборки](how-to/build/index.md) описывает пресеты, зависимости, выбор
  цели и обычный цикл настройки и сборки.
- [Процесс генерации](how-to/build/generated-content.md) задаёт порядок codegen,
  скриптов, ресурсов, метаданных, документации и артефактов сайта.
- [Матрица поддержки](reference/platforms/support-matrix.md) разделяет
  проверенные, smoke-проверенные, доступные в исходниках, экспериментальные и
  неподдерживаемые комбинации.
- [Упаковка и выпуск](how-to/release/packaging.md) определяет объявления пакетов,
  payload, происхождение, границы подписания, приёмку, публикацию и откат.
- [Безопасность и секреты](how-to/release/security-and-secrets.md),
  [релизные операции](how-to/release/operations.md) и
  [резервное копирование и восстановление](how-to/release/backup-and-recovery.md)
  описывают переиспользуемые эксплуатационные границы выпуска.
- [Обновление движка](how-to/migration/engine-upgrade.md) задаёт сверку полного
  диапазона ревизий, проверку совместимости, регенерацию и откат.

## Создание контента

- [Формат прототипов](how-to/content/prototype-format.md)
- [Формат карт](how-to/content/map-format.md)
- [Формат моделей](how-to/content/model-format.md) и
  [анимация моделей](how-to/content/model-animation.md)
- [Форматы изображений и спрайтов](how-to/content/image-format.md) и
  [корневое движение спрайтов](how-to/content/sprite-root-motion.md)
- [Текст и локализация](how-to/content/text-and-localization.md)
- [Формат эффектов](how-to/content/effect-format.md)
- [Формат и runtime частиц](how-to/content/particle-format.md)
- [Форматы шрифтов и компоновка текста](how-to/content/font-format.md)
- [Аудио](how-to/content/audio.md) и [видео](how-to/content/video.md)

Каждое руководство ссылается на сгенерированный справочник, если движок владеет
декларативной грамматикой или машиночитаемым контрактом. Игра владеет своими
каталогами, балансом, заданиями, диалогами, визуальной политикой и политикой
локализации.

## Устройство среды выполнения

- [Архитектура движка](explanation/architecture/index.md) и
  [дерево исходников](contributing/source-tree/index.md) объясняют, где должен
  находиться код конкретного поведения.
- [Модель сущностей и свойств](explanation/entity-and-property-model/index.md),
  [карты, движение и геометрия](explanation/maps-and-movement.md),
  [полномочия и сеть](explanation/authority-and-networking/index.md) и
  [хранение данных](explanation/persistence/index.md) — фасад базы, очередь
  commit, согласованные с backend снимки и журналы восстановления — описывают
  переиспользуемую модель мира.
- [Клиентская среда выполнения](explanation/runtime/client.md),
  [серверная среда выполнения](explanation/runtime/server.md),
  [frontend и рендеринг](explanation/rendering/index.md) и
  [разделение клиента и обновление](explanation/runtime/client-updater.md)
  описывают процессы и слой представления.
- [Среда выполнения скриптов](explanation/scripting-runtime/index.md),
  [жизненный цикл и конкурентность скриптов](how-to/scripting/lifecycle-and-concurrency.md),
  [стиль и рефакторинг AngelScript](how-to/scripting/style-and-refactoring.md) и
  [удалённые вызовы](reference/scripting/remote-calls.md) задают переиспользуемый
  скриптовый контракт.
- [Среда выполнения GUI](how-to/runtime/gui.md) описывает регистрацию экранов,
  жизненный цикл, компоновку, рисование, ввод и проектные hooks.

## Инструменты и проверка изменений

- [Интерактивное руководство Mapper](how-to/tools/mapper-interactive.md) описывает
  повседневное редактирование карт, историю, безопасное сохранение и визуальную
  проверку.
- [Инструменты Mapper](how-to/tools/mapper.md) описывают жизненный цикл Mapper,
  автоматизацию скриптами, детерминированные снимки и headless-интеграцию.
- [Инструменты создания частиц](how-to/tools/particle-authoring.md) и
  [просмотрщики анимации и частиц](how-to/tools/animation-particle-viewers.md)
  описывают сфокусированную визуальную проверку.
- [Игровое и интеграционное тестирование](how-to/testing/gameplay-and-integration.md),
  [тестирование](contributing/testing/index.md),
  [отладка](troubleshooting/debugging.md) и
  [профилирование](how-to/quality/profiling.md) помогают выбрать достаточные
  доказательства для изменения.
- [Протокол AiControl](how-to/ai-control-protocol.md) и
  [исполняемый пример протокола](../../Examples/AiControlSample/README.ru.md)
  определяют нейтральный к проекту транспорт автоматизации и его границу
  безопасности.

## Справочники

- [Приложения](reference/applications.md) перечисляют точки входа исполняемых
  файлов и библиотек.
- [Интерфейс проекта CMake](reference/cmake/index.md) содержит точные параметры,
  стадии, hooks и доступные проекту helper-функции.
- [CLI BuildTools](reference/buildtools/index.md) и
  [CLI вспомогательных инструментов](reference/helper-cli/index.md) содержат
  полученные из парсеров команды, аргументы, значения по умолчанию, варианты и
  точный вывод справки.
- [Сгенерированные API и метаданные](reference/metadata/index.md) описывают
  поток аннотаций исходного кода, codegen, сгенерированных моделей и публикации.
- [Essentials](reference/native/essentials.md) задаёт нативный фундамент,
  строгий порядок include, словарь выделения памяти и карту подсистем.
- [Индекс публичных контрактов](reference/public-contract/index.md) связывает все домены
  сгенерированных контрактов и их метки стабильности.
- [Владение методами Script API](reference/script-api/method-ownership.md)
  группирует нативные экспорты скриптов по стороне выполнения и типу получателя.
- [Справочник нативных расширений](reference/native-extension/index.md) описывает
  роли, hooks, fallback и сгенерированные привязки.
- [Матрица поддержки платформ](reference/platforms/support-matrix.md) фиксирует
  доказательства для заявлений о поддержке.

Канонические машиночитаемые модели находятся в
[`Docs/generated/`](../generated/). ИИ-клиентам следует начинать с
[`llms.txt`](../../llms.txt), использовать
[`docs-manifest.json`](../../docs-manifest.json) для стабильных метаданных
документов и загружать [`llms-full.txt`](../../llms-full.txt), только когда
нужен ограниченный самостоятельный корпус.

## Сопровождение движка и документации

- [Сопровождение документации](contributing/documentation/index.md) задаёт
  владение источниками, сверку ревизий, порядок регенерации и доказательства для
  рецензирования.
- [Процесс перевода](contributing/documentation/translation.md) задаёт английский
  источник, русское зеркало, глоссарий, хеши актуальности и паритет кода.
- [Публикация сайта](contributing/documentation/site-publication.md) определяет
  маршрут GitHub Pages/Jekyll, локальный просмотр, собранный артефакт и
  production-проверки `fonline.ru`.
- [Управление изменениями контрактов](contributing/contract-change-management.md)
  классифицирует изменения сгенерированных моделей и решения по ломающим
  изменениям.
- [Проверка примеров документации](contributing/documentation/snippets.md) и
  [оценка документации для ИИ](contributing/documentation/ai-evaluation.md)
  задают проверки исполняемых примеров, поиска и доказательств.
- [Контракты нативного кода](contributing/coding-contracts/) и
  [сопровождение сторонних библиотек](contributing/third-party/) определяют
  низкоуровневые правила для разработчиков движка.

Активный production-план и история проверок остаются в
[`Docs/ProductionDocumentationPlan.md`](https://github.com/cvet/fonline/blob/master/Docs/ProductionDocumentationPlan.md),
[`Docs/_meta/DocumentationBacklog.md`](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationBacklog.md) и
[`Docs/_meta/DocumentationVerificationReport.md`](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationVerificationReport.md)
до запланированного переноса в `_meta/` с долговечными редиректами.

## Граница ответственности

Документация движка владеет переиспользуемым поведением среды выполнения,
инструментами, сборочными и платформенными контрактами, форматами,
сгенерированными API и соглашениями нативного и скриптового кода. Подключающий
проект владеет конкретным игровым контентом, правилами продукта, политикой
развёртывания, учётными данными сервисов и проектными командами.

Нормативные процедуры Engine должны выполняться из отдельной копии движка и не
должны зависеть от файлов Last Frontier, TLA или другого проекта. Внешние
проекты могут давать привязанные к ревизии доказательства, но переиспользуемые
helpers и регрессионные тесты, на которых основана гарантия Engine, должны
находиться в этом репозитории.
