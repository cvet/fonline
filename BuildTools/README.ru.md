---
layout: default
title: Инструменты сборки FOnline Engine
permalink: /BuildTools/README.ru.html
locale: ru
document_id: buildtools-readme
---

<!-- docs-translation: {"document_id":"buildtools-readme","locale":"ru","source_path":"BuildTools/README.md","source_sha256":"67f80b75ccbd6de7afbebd38e5978b08dffefb7f5132ef5822bf5cb9354a41d4"} -->

# Инструменты сборки FOnline Engine

## Скрипты сборки

Сборка обычно начинается во встраивающем проекте. Поддерживаемый рабочий
процесс описан в [Build Workflow](../Docs/ru/how-to/build/), а этот файл содержит
команды и входные переменные, относящиеся к BuildTools.

Точная поверхность команд основного `buildtools.py` генерируется из его
исполняемого parser. Используйте [справочник BuildTools CLI](../Docs/ru/reference/buildtools/index.md)
или [каноническую JSON-модель](../Docs/generated/cli.json), а не поддерживайте
отдельный список команд вручную.

Скрипты-помощники движка имеют отдельную поверхность на основе manifest.
Используйте [справочник helper CLI](../Docs/ru/reference/helper-cli/index.md) или
[каноническую JSON-модель](../Docs/generated/helper-cli.json) для codegen,
компиляции Mono, покрытия, оркестрации gameplay-тестов, проверки импортов
Windows 7, Android-device, локального web-сервера и MSI-команд. Исполняемые
parser владеют синтаксисом; `HelperCliInterface.json` владеет назначением,
аудиторией, владельцем вызова и явной границей с основным CLI и packager.

`gameplay_test_runner.py` выполняет проверенные multi-process smoke manifest с
ожиданием готовности, обязательными/запрещёнными маркерами, единым deadline
сценария, очисткой и компактным JSON-отчётом. Его переиспользуемый тестовый
контракт и реальное доказательство headless server/client описаны в
[Gameplay- и integration-тестирование](../Docs/ru/how-to/testing/gameplay-and-integration.md).

`ai_control_client.py` является эталонным клиентом стандартной библиотеки для
экспериментального независимого от проекта конверта AiControl NDJSON/TCP. Он
читает общие токены из переменной окружения, отклоняет удалённые endpoint без
явного разрешения и проверяется через [протокол AiControl](../Docs/ru/how-to/ai-control-protocol.md)
и запускаемый [образец протокола](../Examples/AiControlSample/README.ru.md).

Возможности деклараций пакетов и payload, реализованные в `package.py`,
моделируются в `PackageInterface.json`, проверяются по реализации и отображаются
в [справочнике package interface](../Docs/ru/reference/packages/index.md).
Конкретная матрица `DefinePackage(...)` встраивающего проекта остаётся
собственностью проекта.

Роли и hooks нативных расширений моделируются в
`NativeExtensionInterface.json`, проверяются по текущему поведению CMake/codegen
и отображаются в [справочнике нативных расширений](../Docs/ru/reference/native-extension/index.md).
Рекомендации по authoring и совместимости находятся в [Нативные расширения](../Docs/ru/how-to/native-extensions.md).

Грамматика прототипов и правила валидации версионируются в
`PrototypeFormatInterface.json`; `docs_prototype_format.py` проверяет живые
parser/baker anchors, выводит встроенный каталог metadata properties и строит
[справочник формата прототипов](../Docs/ru/reference/prototype-format/index.md).
Руководство по authoring и миграции находится в
[Формат прототипов](../Docs/ru/how-to/content/prototype-format.md).

Грамматика карт, владение размещением, нормализация Mapper и правила baking по
сторонам версионируются в `MapFormatInterface.json`; `docs_map_format.py`
проверяет живые loader/baker/mapper anchors, выводит встроенный каталог свойств
Map/Critter/Item и строит [справочник формата карт](../Docs/ru/reference/map-format/index.md).
Руководство по authoring находится в [Map Format](../Docs/ru/how-to/content/map-format.md).

Грамматика описания моделей, входные mesh, правила композиции и compile-time
лимиты версионируются в `ModelFormatInterface.json`; `docs_model_format.py`
сравнивает все допустимые написания с `ModelDescriptionParser::ParseToken` и
строит [справочник формата моделей](../Docs/ru/reference/model-format/index.md).
Руководство по authoring находится в [формате моделей](../Docs/ru/how-to/content/model-format.md).

Синтаксис text pack, нормализация языков, `$Text` прототипов, runtime lookup и
цветовые теги renderer версионируются в `TextFormatInterface.json`;
`docs_text_format.py` проверяет живые source anchors, выводит языки по умолчанию
и имена генерируемых пакетов прототипов и строит
[справочник текстового формата](../Docs/ru/reference/text-format/index.md).
Руководство по authoring находится в
[Текст и локализация](../Docs/ru/how-to/content/text-and-localization.md).

Секции эффектов, состояние pass/render, встроенные shader resources, backend
outputs, runtime caching и script control версионируются в
`EffectFormatInterface.json`; `docs_effect_format.py` проверяет живые
baker/renderer/runtime anchors, выводит compile-limit defaults и строит
[справочник формата эффектов](../Docs/ru/reference/effect-format/index.md).
Руководство по authoring находится в [Effect Format](../Docs/ru/how-to/content/effect-format.md).

Форматы исходных изображений, композиция FOFRM, legacy selectors имён файлов,
записи baked sprite, стандартные runtime factories, atlases, caches и
валидация версионируются в `ImageFormatInterface.json`; `docs_image_format.py`
проверяет живые baker/client/test anchors, выводит оба extension registry и
строит [справочник изображений](../Docs/ru/reference/image-format/index.md).
Руководство по authoring находится в
[Форматы изображений и спрайтов](../Docs/ru/how-to/content/image-format.md).

Particle XML, зарегистрированные объекты SPARK, `SparkQuadRenderer`, поведение
редактора, raw-copy delivery, runtime caches/render paths и интеграции
версионируются в `ParticleFormatInterface.json`; `docs_particle_format.py`
выводит живые registry, descriptors, покрытие редактора, extensions и settings
и строит [справочник particle format](../Docs/ru/reference/particle-format/index.md).
Руководство по authoring находится в
[Формат и исполнение частиц](../Docs/ru/how-to/content/particle-format.md).

Дескрипторы FOFNT/BMFont, привязка font slot, масштабирование при bind, layout
текста, rendering flags, inline colors и валидация версионируются в
`FontFormatInterface.json`; `docs_font_format.py` проверяет parser, resource,
enum, atlas, cache и bundled-descriptor anchors и строит
[справочник формата шрифтов](../Docs/ru/reference/font-format/index.md).
Руководство по authoring находится в
[форматы шрифтов и компоновка текста](../Docs/ru/how-to/content/font-format.md).

Доставка WAV/ACM/Ogg, лимиты decoder, identities эффектов и нумерованные
варианты, музыка по точному пути, repeat timing, frontend mixing и поведение
silent/headless версионируются в `AudioInterface.json`; `docs_audio.py` выводит
живые resource, decoder, frontend, setting и test evidence и строит
[справочник audio](../Docs/ru/reference/audio/index.md). Руководство по authoring
находится в [Audio Resources and Playback](../Docs/ru/how-to/content/audio.md).

Доставка Ogg/Theora, декодирование ресурса целиком, поведение fullscreen
queue/input/music, embedded playback, rendering и пробелы в проверках
версионируются в `VideoInterface.json`; `docs_video.py` выводит живые decoder,
client, script, rendering, raw-copy, dependency и test evidence и строит
[справочник video](../Docs/ru/reference/video/index.md). Руководство по интеграции
находится в [Video Resources and Playback](../Docs/ru/how-to/content/video.md).

Переиспользуемые типы AngelScript GUI, документированные members/callbacks,
screen API, annotations, lifecycle, layout, drawing, input и embedding hooks
версионируются в `GuiRuntimeInterface.json`; `docs_gui_runtime.py` выводит живой
контракт CoreScripts и строит
[справочник GUI runtime](../Docs/ru/reference/gui-runtime/index.md). Руководство по
интеграции находится в [GUI Runtime](../Docs/ru/how-to/runtime/gui.md). Декларативные GUI
formats и generators остаются за пределами этого контракта Engine.

Владение публичными примерами, порядок, точные Engine pins, compatibility lanes,
governance files, исключения source staging и release gates версионируются в
`Examples/PublicRepositories.json`; `docs_examples.py` проверяет общий overlay,
материализует чистый review candidate только когда точный checkout Engine чист
и доступен удалённо, создаёт
[реестр публичных примеров](../Docs/ru/reference/public-examples/index.md) и
проверяет внешние candidate repositories перед публикацией.

Сценарии валидации можно запускать по одному или группой одной командой:

```bash
Engine/BuildTools/validate.sh unit-tests
Engine/BuildTools/validate.sh android-arm64-client linux-client linux-server
```

Python regression tests BuildTools находятся в `Engine/BuildTools/tests/` и
могут запускаться напрямую:

```bash
pytest -q Engine/BuildTools/tests
```

## Структура CMake

Все внутренние CMake modules находятся в `Engine/BuildTools/cmake`.
Публичная точка входа `Init.cmake` сохранена в корне `Engine/BuildTools`;
поэтапная реализация CMake находится в `Engine/BuildTools/cmake/stages/`, а
helpers - в `Engine/BuildTools/cmake/helpers/`.
`cmake/ProjectInterface.json` является версионированной документационной моделью
project options, порядка stages, entrypoints, hooks и выбранной поверхности
helpers. Configure-time authority остаётся у `Init.cmake` и файлов стадий и
helper-команд; структурный тест отклоняет расхождения реализации с моделью.
Используйте сгенерированный
[справочник CMake project interface](../Docs/ru/reference/cmake/index.md) или его
[каноническую JSON-модель](../Docs/generated/cmake.json), а не копируйте
декларации из файлов реализации stages. Для принадлежащих проекту targets и
SDK используйте [Проектные зависимости](../Docs/ru/how-to/native-extensions/project-dependencies.md).
Текущая ревизия не имеет объявленной helper-команды project libraries и
использует привязанное к ревизии integration state `FO_*_LIBS`.
Исполняемый opt-in starter project находится в `Engine/Examples/MinimalProject`
и предоставляет свой локальный validator. Он не является target текущего
обязательного workflow Engine.

## Генераторы документации

- `docs_api.py` запускает существующий metadata parser `codegen.py` в режиме только чтения, применяет source-owned классификации `///@ ApiContract` и записывает/проверяет `Docs/generated/api.json`.
- `docs_api_diff.py` предоставляет слой сравнения native symbols, включая overload families и source-owned stability.
- `docs_contract_diff.py` сравнивает модели native API, CMake, main CLI, package, helper CLI, native-extension, prototype-format, map-format, model-format, text-format, effect-format, image-format, particle-format, font-format, audio, video, GUI runtime и AiControl protocol в одном отчёте по паре revisions и требует точных dispositions общего ledger для baseline-public или model-contract breaks.
- `docs_public_api.py` записывает/проверяет канонические EN/RU индексы контрактов и корневой legacy-маршрут `PUBLIC_API.md` из всех восемнадцати машинных моделей и их сгенерированных human references; native inventory counts и default stability никогда не поддерживаются вручную.
- `docs_external_evidence.py` проверяет pinned Last Frontier/TLA discovery inventory, owner review policy, dispositions, priorities, Engine/planned targets и детерминированный внутренний отчёт. При наличии обоих точных checkout он также доказывает через `git cat-file`, что каждый записанный путь существует в pinned commit.
- `docs_cmake.py` проверяет `cmake/ProjectInterface.json` и записывает/проверяет `Docs/generated/cmake.json`, английский справочник Markdown в `Docs/en/reference/cmake/` и устойчивые указатели маршрутов в `Docs/generated/cmake/`.
- `docs_cli.py` загружает исполняемый parser `argparse` из `buildtools.py` и записывает/проверяет `Docs/generated/cli.json`, точный английский справочник Markdown на основе help в `Docs/en/reference/buildtools/` и устойчивые указатели маршрутов в `Docs/generated/cli/`.
- `docs_helper_cli.py` проверяет `HelperCliInterface.json`, доказывает полное покрытие inventory `create_parser()` и записывает/проверяет `Docs/generated/helper-cli.json`, точный английский справочник Markdown на основе help в `Docs/en/reference/helper-cli/` и устойчивые указатели маршрутов в `Docs/generated/helper-cli/`.
- `docs_native_extension.py` проверяет документационную модель native roles/hooks по текущим данным CMake/codegen и записывает/проверяет `Docs/generated/native-extension.json` вместе со страницами roles, hooks и bindings.
- `docs_prototype_format.py` проверяет `PrototypeFormatInterface.json` по живым parser/baker sources, выводит применимость встроенных entity/property из metadata и записывает/проверяет `Docs/generated/prototype-format.json` вместе со страницами syntax, property и validation.
- `docs_map_format.py` проверяет `MapFormatInterface.json` по живым loader/baker/mapper/runtime sources, выводит данные Map/Critter/Item property и ItemOwnership из metadata и записывает/проверяет `Docs/generated/map-format.json` вместе со страницами syntax, property, baking и validation.
- `docs_model_format.py` проверяет `ModelFormatInterface.json` по живому model parser, mesh baker, client runtime, limits и tests, затем записывает/проверяет `Docs/generated/model-format.json` вместе со страницами syntax, token, composition, asset, animation и validation.
- `docs_text_format.py` проверяет `TextFormatInterface.json` по text-pack, baker, runtime, script API, settings, renderer и test sources, затем записывает/проверяет `Docs/generated/text-format.json` вместе со страницами syntax, language, prototype-text, runtime и validation.
- `docs_effect_format.py` проверяет `EffectFormatInterface.json` по effect baker, render-effect/runtime/cache/script API, backend conventions, project limits и tests, затем записывает/проверяет `Docs/generated/effect-format.json` вместе со страницами syntax, render-state, resource, baking, runtime и validation.
- `docs_image_format.py` проверяет `ImageFormatInterface.json` по ImageBaker, FOFRM/import sources, стандартному client factory/sheet/atlas/cache behavior и tests, затем записывает/проверяет `Docs/generated/image-format.json`, канонические английские страницы в `Docs/en/reference/image-format/` и совместимые маршруты в `Docs/generated/image-format/`.
- `docs_particle_format.py` проверяет `ParticleFormatInterface.json` по raw-copy settings, SPARK XML/registry/descriptors, Engine renderer, ParticleEditor, client runtime, script/model integrations и tests, затем записывает/проверяет `Docs/generated/particle-format.json` вместе со страницами XML, object, renderer, tooling, runtime, integration и validation.
- `docs_audio.py` проверяет `AudioInterface.json` по raw-copy settings, resource indexing, декодированию WAV/ACM/Ogg, script playback, frontend conversion/mixing, headless behavior и native-test inventory, затем записывает/проверяет `Docs/generated/audio.json` вместе со страницами format, delivery, decoding, playback и validation.
- `docs_video.py` проверяет `VideoInterface.json` по raw-copy settings, декодированию Ogg/Theora, fullscreen queue/input/music/drawing, embedded script playback, renderer behavior, dependencies и native-test inventory, затем записывает/проверяет `Docs/generated/video.json` вместе со страницами format, delivery, decoding, fullscreen, embedded и validation.
- `docs_gui_runtime.py` проверяет `GuiRuntimeInterface.json` по `Gui.fos`, `Input.fos`, native client dispatch, tutorial boundaries и test inventory, затем записывает/проверяет `Docs/generated/gui-runtime.json` вместе со страницами type, screen API, lifecycle, layout/rendering, input и integration/validation.
- `docs_ai_control_protocol.py` проверяет `AiControlProtocol.json` по reference client и запускаемому sample, затем записывает/проверяет `Docs/generated/ai-control-protocol.json` вместе со страницами wire, method, command/event, security и integration/validation.
- `docs_package.py` проверяет документационную модель package и исполняемый parser `package.py`, затем записывает/проверяет `Docs/generated/package.json` вместе со страницами package reference.
- `docs_examples.py` проверяет `Examples/PublicRepositories.json` и governance overlay, записывает/проверяет `Docs/generated/public-examples.json` и его registry page, материализует source-ready example в новом чистом candidate directory и проверяет metadata внешнего repository, точные gitlink pins, обязательные файлы и байты provenance file.
- `docs_reference.py` отображает каноническую модель API в совместимый с GitHub Pages Markdown в `Docs/generated/api/`.
- `docs_metadata.py` строго декодирует project-baked `Metadata.fometa-server/client`, проверяет согласованность обеих сторон и записывает/проверяет принадлежащий проекту JSON/Markdown catalog remote calls.
- `docs_inventory.py` записывает/проверяет независимый inventory export methods, native tests и setting declarations.
- `docs_localization.py` обеспечивает полное двуязычное покрытие, проверяет glossary, стабильные locale targets, нормализованные English hashes, точные переведённые fences и language-preserving links, затем записывает/проверяет `Docs/generated/translation-status.json`.
- `docs_description_translations.py` инвентаризирует обращённый к читателю текст в 20 генерируемых контрактных моделях, применяет проверенный русский overlay со стабильными ID, отклоняет повторяющиеся, неизвестные, устаревшие, меняющие тип или код записи и записывает/проверяет `Docs/generated/description-translation-status.json`. Отсутствующие записи остаются явными, пока семантический каталог не сможет перейти из `registered-translations-current` в `complete`.
- `docs_ai_delivery.py` проецирует `Docs/documentation-manifest.json` и канонический Markdown в корневые `llms.txt`, ограниченный `llms-full.txt` и публичный `docs-manifest.json`; он нормализует content hashes и отклоняет stale, oversized или non-deterministic output.
- `docs_site.py` разрешает manifest-owned stable document IDs в проверенные localized Jekyll navigation data, ограниченные статические English/Russian search indexes и публичный version/locale/legacy-route catalog; он отклоняет неизвестные, дублированные или пропущенные top-level pages, route collisions, неоднозначные canonical targets, отсутствующие locale pairs, cross-locale search ownership и oversized или stale output.
- `docs_ai_eval.py` проверяет версионированный standalone task set в `Docs/ai-evaluation.json` по manifest и той же компактной search model, которую использует browser, затем записывает/проверяет `Docs/generated/ai-evaluation-report.json` с ranks, evidence checks, success rate и MRR.
- `docs_ai_model_eval.py` необязательно запускает изолированные задачи через локальную модель Ollama, сохраняя точные хеши inputs/prompts/model, потоковые raw attempts, наблюдения evidence и возобновляемое состояние задач в игнорируемом `Workspace/ai-evaluation/`. Он повторяет ограниченные дословные decision- и query-relevant-разделы найденных документов-кандидатов рядом с инструкцией ответа, сохраняя rubric ответа скрытым; это локальный инструмент проверяемых свидетельств, а не CI dependency.
- `docs_ai_model_review.py` создаёт, финализирует и проверяет компактные semantic reviews для raw-прогонов семейств моделей; `--require-run` дополнительно доказывает сохранённый raw SHA-256 и встроенные метаданные прогона.
- `docs_snippets.py` инвентаризует каждый fence в public/current/human documents, применяет language/harness contract из `SnippetPolicy.json`, записывает/проверяет `Docs/generated/snippets.json` и при необходимости требует реальные parser Bash и PowerShell без выполнения команд.
- `docs_diagrams.py` проверяет `DocumentationDiagrams.json`, alt/caption markup owning document, source provenance, canvas bounds, node overlap и local-only SVG safety; он записывает/проверяет desktop/mobile SVG variants для трёх teaching diagrams вместе с `Docs/generated/diagrams.json` с точными hashes.
- `docs_screenshots.py` проверяет `DocumentationScreenshots.json`, точные PNG bytes/dimensions, alt/caption markup owning document, capture environment, source provenance и recapture triggers; он записывает/проверяет `Docs/generated/screenshots.json` с точными image, manifest и source hashes.
- `docs_site_artifact.py` проверяет готовое дерево Jekyll `_site` по route catalog и source artifacts. Он отклоняет отсутствующие current/available-locale routes, изменённые или отсутствующие static endpoints, неверные canonical URL/languages, отсутствующие accessibility landmarks/names, duplicate IDs и сломанные ссылки на публикуемые локальные ресурсы; CI сохраняет его JSON report.
- `docs-browser/audit.mjs` локально обслуживает готовое дерево `_site` и применяет зафиксированный lock-файлом Playwright Chromium вместе с axe-core для проверки каждого route на desktop и mobile ширине. Он отклоняет нарушения WCAG 2.2 A/AA, runtime/resource errors, горизонтальную прокрутку уровня страницы, сломанный responsive layout и keyboard failures в skip navigation, search, theme, copy и mobile focus-trapped drawer; CI сохраняет JSON и screenshots.
- `docs_validate.py` проверяет documentation manifest, локальные links/anchors, source ownership, Pages contract и freshness каждого generated artifact.

Запускайте их сфокусированные тесты и проверки из корня движка; generated JSON
и Markdown хранятся в repository и не должны редактироваться вручную.

Материализуйте review candidate только из чистого, удалённо доступного точного
Engine commit:

```bash
python BuildTools/docs_examples.py --stage-repository minimal-multiplayer --engine-revision "$(git rev-parse HEAD)" --output Workspace/fonline-minimal-multiplayer
```

```bash
python -m unittest discover -s BuildTools/tests -p "test_docs*.py"
cmake -P BuildTools/tests/validate_project_interface.cmake
cmake -P BuildTools/tests/validate_native_extension_interface.cmake
cmake -P BuildTools/tests/validate_package_interface.cmake
python BuildTools/docs_cmake.py --check
python BuildTools/docs_cli.py --check
python BuildTools/docs_helper_cli.py --check
python BuildTools/docs_native_extension.py --check
python BuildTools/docs_prototype_format.py --check
python BuildTools/docs_map_format.py --check
python BuildTools/docs_model_format.py --check
python BuildTools/docs_text_format.py --check
python BuildTools/docs_effect_format.py --check
python BuildTools/docs_image_format.py --check
python BuildTools/docs_particle_format.py --check
python BuildTools/docs_font_format.py --check
python BuildTools/docs_audio.py --check
python BuildTools/docs_video.py --check
python BuildTools/docs_gui_runtime.py --check
python BuildTools/docs_ai_control_protocol.py --check
python BuildTools/docs_package.py --check
python BuildTools/docs_examples.py --check
python BuildTools/docs_public_api.py --check
python BuildTools/docs_external_evidence.py --check
python BuildTools/docs_inventory.py --check
python BuildTools/docs_localization.py --check --enforce-complete
python BuildTools/docs_description_translations.py --check
python BuildTools/docs_diagrams.py --check
python BuildTools/docs_screenshots.py --check
python BuildTools/docs_snippets.py --check --external
python BuildTools/docs_site.py --check
python BuildTools/docs_ai_eval.py --check
python BuildTools/docs_site_artifact.py --site-dir _site
npm ci --prefix BuildTools/docs-browser
npx --prefix BuildTools/docs-browser playwright install chromium
npm --prefix BuildTools/docs-browser run audit
python BuildTools/docs_ai_delivery.py --check
python BuildTools/docs_validate.py
```

### Компилятор проектов Effekseer

`Source/Tools/EffekseerCompiler.h/.cpp` является нативным C++ module,
скомпилированным в `BakerLib`. Для каждого фиксированного Editor-1.80.5 /
project-version-3 `.efkproj` он проверяет XML profile и возвращает raw `SKFE`
bytes вместе со ссылочными textures, models, sounds и curves. `ParticleBaker`
вызывает module напрямую и проверяет каждый результат vendored Effekseer Core
перед публикацией.

`ParticleBaker` разрешает dependency paths внутри физического directory resource
source проекта и хранит snapshot path/size/write-time для каждого эффекта в
`<BakeOutput>/.baker-cache/Effekseer/`. Исходный проект и dependency snapshot
независимо инвалидируют производный `.efk`; после изменений кода compiler
используйте `ForceBakeResources`. Изменённый эффект перекомпилируется по
требованию без invalidation несвязанных эффектов во время focus-triggered
resource reindex Mapper.

Compiler не линкуется в runtime clients и не упаковывается с ними. Нативные
Baker и Mapper hosts используют его, когда derived resources устарели; Web
clients используют заранее подготовленные host `.efk` resources.

### Developer bundle Effekseer Editor

Зафиксированный upstream Effekseer Editor собирается как отдельный Windows
win64 developer tool. Он независим от `FO_EFFEKSEER_PARTICLES` и не представлен
как Engine CMake option, application target или универсальный target
`buildtools.py build`. Поэтому runtime builds никогда не получают toolchain
Editor или его Viewer/UI libraries.

Собирайте и размещайте его через общую точку входа auxiliary tools:

```powershell
$env:FO_OUTPUT = (Get-Location).Path
python Engine\BuildTools\buildtools.py build-auxiliary effekseer-editor Release
```

Скрипт собирает managed .NET 10 UI и нативные Viewer/material tools в
изолированных output directories, затем размещает self-contained payload.
Languages, fonts, icons, meshes, `LICENSE_TOOL`, material editor и Direct3D
11/OpenGL material compilers являются частью payload. Запись GIF отключена в
этой сборке FOnline, что исключает неиспользуемую зависимость libgd; обычное
редактирование и интерактивный preview Editor остаются доступными.

Адаптация FOnline ориентирована на исходники. **Save** и **Save As** в Editor
принимают только `.efkproj` и атомарно записывают нормализованный UTF-8 XML без
BOM. Stock preview Editor предназначен для authoring iteration; preview Mapper
встраивающего проекта остаётся финальным путём проверки через renderer FOnline
и capability gate.

Переиспользуемая package schema не имеет Effekseer-specific binary role.
Встраивающий проект доставляет отдельно размещённый tool через общую package
declaration `INCLUDE <source-path-glob> <target-path-in-pack>`. Source globs
задаются относительно `FO_OUTPUT_PATH`; packager заменяет принадлежащее ему
target tree и обновляет существующий `SingleZip` без duplicate или stale entries.

## Переменные окружения сборки

Build scripts (sh/bat) можно вызывать из текущего каталога (например,
`./linux.sh`) или из корня repository (например, `BuildTools/linux.sh`).
Перед запуском build scripts можно задать следующие переменные окружения.

BuildTools использует только project-specific overrides с префиксом `FO_`.
Любой объявленный project-interface option можно переопределить одноимённой
переменной окружения; точный список и precedence находятся в
[сгенерированном справочнике options](../Docs/ru/reference/cmake/options.md).
Стандартные tool variables `ANDROID_HOME`, `ANDROID_SDK_ROOT` и
`ANDROID_NDK_ROOT` зарезервированы для запускаемых внешних инструментов и не
читаются как inputs BuildTools.

#### FO_ENGINE_ROOT

Путь к корневому каталогу repository FOnline.
Если не задан, используется каталог на один уровень выше запускаемого script
file (вне `BuildTools`, в корне repository).

*По умолчанию: `$(dirname ./script.sh)/../`*
*Пример: `export FO_ENGINE_ROOT=/mnt/d/fonline`*

#### FO_WORKSPACE

Путь к каталогу хранения всех промежуточных build files.
По умолчанию сборка выполняется в каталоге `Workspace` внутри текущего каталога.

*По умолчанию: `$PWD/Workspace`*
*Пример: `export FO_WORKSPACE=/mnt/d/fonline-workspace`*

#### FO_ANDROID_HOME / FO_ANDROID_SDK_ROOT / FO_ANDROID_NDK_ROOT

Необязательные явные overrides Android SDK и NDK для BuildTools. Если они не
заданы, BuildTools использует подготовленные workspace locations, а затем
системный fallback `/usr/lib/android-sdk` в Linux, когда он доступен.

#### FO_CLANG_FORMAT

Необязательный явный путь к binary `clang-format`, используемому
`buildtools.py format-source`. Binary всё равно должен проходить gate версии 20.
Если переменная не задана, BuildTools ищет в системном `PATH` сначала
`clang-format-20`, затем `clang-format`. Встраивающий проект может направить
formatter Engine на собственный pinned или bundled binary.

## Подготовка общего workspace

Общие части workspace подготавливаются через `buildtools.py` и оборачиваются
platform-specific scripts.

- Linux: `Engine/BuildTools/prepare-workspace.sh`
- macOS: `Engine/BuildTools/prepare-mac-workspace.sh`
- Windows: `Engine/BuildTools/prepare-win-workspace.ps1`

Сейчас общий flow охватывает:

- `toolset`
- `emscripten`
- `android-ndk`
- `dotnet`
- `xwin`
- `msan-libcxx`

Установка системных пакетов Linux является явной и отделена от подготовки
workspace:

- `common-packages`
- `linux-packages`
- `web-packages`
- `android-packages`
- `windows-cross-packages`
- `msi-packages`
- `all-packages`

Workspace features `linux`, `web`, `android-arm64` и `windows-cross` не
устанавливают apt packages. На чистом host сначала передайте соответствующий
feature `*-packages`. `all-packages` устанавливает все группы выше, включая
`msi-packages` с toolset MSI installer `wixl`. Поскольку apt существует только
на host-provisioning path, ни одна часть `prepare-workspace` не устанавливает
системные пакеты, а параллельные CI jobs не конкурируют за apt lock.

Проверки host prerequisites также доступны через основной tool:

- `buildtools.py host-check linux`
- `buildtools.py host-check macos`
- `buildtools.py host-check windows`

Host wrapper scripts делегируют unified-команде подготовки workspace:

- `buildtools.py prepare-host-workspace linux ...`
- `buildtools.py prepare-host-workspace windows ...`
- `buildtools.py prepare-host-workspace macos ...`

Версия Emscripten фиксируется `Engine/ThirdParty/emscripten` и устанавливается
в `Workspace/emsdk`.

Примеры:

```bash
python3 Engine/BuildTools/buildtools.py prepare-workspace toolset
python3 Engine/BuildTools/buildtools.py prepare-workspace emscripten
python3 Engine/BuildTools/buildtools.py prepare-workspace android-ndk dotnet
python3 Engine/BuildTools/buildtools.py prepare-workspace msan-libcxx
python3 Engine/BuildTools/buildtools.py prepare-workspace toolset emscripten android-ndk dotnet --check
python3 Engine/BuildTools/buildtools.py prepare-host-workspace linux web-packages web dotnet
```

`msan-libcxx` доступен только в Linux и намеренно исключён из feature workspace
`all` по умолчанию, потому что загружает соответствующие исходники LLVM и
собирает `libc++`, `libc++abi` и `libunwind` с MemorySanitizer instrumentation.
Runtime build также передаёт `BuildTools/sanitizers/msan-runtime-ignorelist.txt`,
чтобы libunwind не выдавал self-report для ABI register snapshots во время
разворачивания C++ exception или sanitizer report. Validator
`unit-tests-san-memory` автоматически подготавливает его перед конфигурацией
`San_Memory`; используйте явную workspace-команду только для предварительного
прогрева CI host или отладки runtime build.

Linux hosts могут подготовить Windows cross-compilation SDK/CRT через ту же
обёртку:

```bash
bash Engine/BuildTools/prepare-workspace.sh windows-cross-packages windows-cross
bash Engine/BuildTools/prepare-workspace.sh windows-cross
python3 Engine/BuildTools/buildtools.py prepare-workspace xwin
python3 Engine/BuildTools/buildtools.py build win64 client Release
python3 Engine/BuildTools/buildtools.py build win32 client Release
```

Feature `windows-cross-packages` устанавливает/проверяет Linux prerequisites.
Wrapper feature `windows-cross` и прямая команда `prepare-workspace xwin`
работают только с workspace: они используют версию xwin, зафиксированную в
`Engine/ThirdParty/xwin`, подготавливают деревья SDK/CRT для `x86` и `x86_64` в
`Workspace/xwin` и намеренно пропускают установку системных пакетов для заранее
подготовленных CI hosts. `buildtools.py` сначала выполняет splat основной
архитектуры, затем объединяет library directories вторичной архитектуры из
изолированных splat, чтобы избежать shared-symlink race xwin `0.6.6-rc.2` при
одном multi-arch вызове. Каждый splat передаёт `--http-retry 5`, поэтому
временные ошибки чтения тела Microsoft CDN повторяются до отказа подготовки.

Для `win32` `buildtools.py` передаёт `CMAKE_SYSTEM_PROCESSOR=x86`; toolchain
сохраняет library paths `x86` из xwin и принудительно использует
`clang-cl --target=i686-pc-windows-msvc`, чтобы compiler probes CMake не
создавали x64 objects для x86 link.

## Исправление регистра путей checkout

```bash
python3 Engine/BuildTools/buildtools.py repair-checkout-case
python3 Engine/BuildTools/buildtools.py repair-checkout-case --check
```

Команда рекурсивно, включая submodules, переименовывает элементы рабочего
дерева, чей регистр на диске отличается от имени в git. Case-only rename
корректно записывается в index, но на файловой системе без учёта регистра git
переписывает только имена файлов: существующий каталог навсегда сохраняет старое
написание. Поэтому повторно используемый checkout продолжает отдавать устаревшее
имя, хотя поиск ресурсов и include остаётся регистрозависимым.

Симптом проявляется далеко от причины: присутствующий на диске файл считается
отсутствующим при baking. Обычно страдают долгоживущие checkout на собственных
Windows runners; свежий clone не воспроизводит проблему. Поэтому CI на таких
runners должен выполнять команду сразу после checkout.

`--check` только сообщает о расхождении и завершается с ненулевым кодом, не
изменяя рабочее дерево. Если index одновременно содержит один путь в двух
вариантах регистра, команда называет оба и отказывается что-либо исправлять:
это неоднозначный tracked collision, требующий решения владельца.

## Отладочный workflow Web в Windows

Локальный отладочный flow Web в Windows использует общие команды:

- `buildtools.py build web client RelWithDebInfo`
- `buildtools.py package-web-debug`

Для оптимизированной browser build используйте:

- `buildtools.py build web client Release`

Упакованная browser build создаётся в
`Workspace/web-debug/<ProjectDevName>-Client-<Config>-Web` и может обслуживаться
генерируемым helper `web-server.py`.

## Отладочный workflow Android

Локальный отладочный flow Android использует общие команды.

Поддерживаемые identifiers Android platform: `android-arm32`, `android-arm64`
и `android-x86`.

Развёртывание на device основано на ADB по Wi-Fi. На целевом device должна быть
включена wireless debugging, и при запросе Android он должен быть спарен с этим
host.

- `buildtools.py build android-arm64 client RelWithDebInfo`
- `buildtools.py package-android-debug <ProjectDevName> android-arm64 <Config>`
- `android_device.py --workspace-root Workspace connect`

Сначала необходимо подготовить части workspace Android SDK и NDK. Используйте
`android-packages` только на чистом Linux host, которому ещё нужны системные
пакеты:

- `bash Engine/BuildTools/prepare-workspace.sh android-arm64`
- `bash Engine/BuildTools/prepare-workspace.sh android-packages android-arm64`

Упакованная Android build создаётся в
`Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android` как готовый
к сборке Gradle project. Сборка и развёртывание:

```bash
python3 Engine/BuildTools/android_device.py --workspace-root Workspace connect
cd Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android
./gradlew assembleDebug
python3 Engine/BuildTools/android_device.py --workspace-root Workspace install --apk Workspace/android-debug/<ProjectDevName>-Client-<Config>-Android/app/build/outputs/apk/debug/app-debug.apk
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch --activity com.example.game/.FOnlineActivity

# Pass the host address for a project config that expects a remote server.
python3 Engine/BuildTools/android_device.py --workspace-root Workspace launch-game --activity com.example.game/.FOnlineActivity
```

`launch-game` автоматически определяет LAN IP host, который достигает
выбранного Wi-Fi Android device, и передаёт его как runtime override
`ClientNetwork.ServerHost`. Благодаря этому упакованный client
`RemoteSceneLaunch` подключается обратно к host server без редактирования baked
config files.

Во время выполнения `FOnlineActivity` размещает `assets/Resources` в app files
directory при первом запуске после установки или обновления, затем запускает
движок с абсолютными overrides `Baking.ClientResources` и
`Baking.CacheResources`, указывающими на runtime location.

Версия Android SDK command-line tools фиксируется
`Engine/ThirdParty/android-sdk` и устанавливается в `Workspace/android-sdk`.
BuildTools использует интерфейс `android sdk install` этого пакета с отключённой
метрикой; устаревший путь через `sdkmanager` не используется.

Версия Android NDK фиксируется `Engine/ThirdParty/android-ndk` и
устанавливается в `Workspace/android-ndk`.

Шаблон Gradle project находится в `Engine/BuildTools/android-project/`, фиксирует
Android Gradle Plugin 9.3.0 с Gradle 9.5.0, компилирует исходники Java на уровне
языка 17 и использует tokens `$PLACEHOLDER$`, которые `package.py` заменяет при
packaging.
Значения Android configuration поступают из authored root и выбранного
`SubConfig`; build-host directives `$ENV`/`$FILE` и
`$TARGET_ENV`/`$TARGET_FILE` разрешаются только при packaging, поэтому overrides
sub-config влияют на APK metadata без необходимости помещать signing values в
baked client config. Android SDK, которым нужна application manifest metadata,
могут использовать settings
`Android.ManifestMetaData.<android:name> = <android:value>`; packager создаёт из
них entries `<meta-data>` внутри `<application>`. Gradle setup SDK может
использовать `Android.GradleMavenRepository.<name> = <url>` и
`Android.GradleDependency.<name> = <Gradle dependency statement>` для
добавления package-config-specific Maven repositories и entries
`dependencies { ... }`. Package-specific Java sources могут использовать
`Android.JavaSource.<name> = <path/to/File.java>`; packager копирует каждый
непустой source в namespace сгенерированного app package и заменяет
`$PACKAGE$` / `$CONFIG$`.

Packaging release APK для Android подписывает artifact. Настройте signing через
`Android.Keystore`, `Android.KeystorePassword`, `Android.KeyAlias` и
`Android.KeyPassword` в main config проекта. Для sensitive inputs используйте
`$TARGET_ENV{...}` или защищённый `$TARGET_FILE{...}` для значения, но не для
пути keystore, чтобы `ConfigBaker` не материализовал их. `package.py` передаёт
`Android.KeystorePassword` и `Android.KeyPassword` в Gradle через переменные
окружения `FO_ANDROID_RELEASE_STORE_PASSWORD` и
`FO_ANDROID_RELEASE_KEY_PASSWORD`, а не записывает их в сгенерированный Gradle
project. При ручной сборке сгенерированного Gradle project задайте эти
переменные перед `./gradlew assembleRelease`; если signing settings пусты,
packaging использует fallback на Gradle debug signing key, чтобы
сгенерированные package APK оставались устанавливаемыми на development devices.
Полная граница описана в [Security and Secrets](../Docs/ru/how-to/release/security-and-secrets.md).

При packaging APK Gradle использует `GRADLE_USER_HOME` внутри текущего workspace
output tree вместо общего `~/.gradle`, поэтому параллельные CI package jobs не
конкурируют за глобальные Gradle caches.

`android_device.py` сначала вызывает `adb mdns services`, показывает найденные
Android Wi-Fi endpoints как нумерованный список, кеширует выбранный endpoint в
`Workspace/android-debug/device-endpoint.txt` и предлагает ручной ввод
`IP[:port]`, если discovery ничего не вернул.

После сборки каждый клиентский и серверный пакет ресурсов открывается повторно —
как ZIP-файлы, записанные на диск, так и пакет в памяти, встраиваемый в
исполняемый файл. Packaging проверяет точный список записей и полностью читает
каждую из них через ZIP reader с проверкой CRC, поэтому повреждённый архив
ресурсов останавливает сборку пакета до того, как попадёт в загружаемый клиент
или в источник обновлений сервера.

## Packaging: изменение binary после сборки

Полный workflow declaration, build/bake/package, platforms, signing,
provenance, acceptance и recovery приведён в
[Упаковка и выпуск](../Docs/ru/how-to/release/packaging.md). Эта секция описывает
детали реализации BuildTools.

`package.py` внедряет несколько значений в уже слинкованные binaries вместо
повторной компиляции для каждого package. Каждая цель является placeholder
фиксированной ёмкости, зарезервированным во время компиляции через
`FO_KEEP_DATA_SYMBOL`, чтобы linker сохранил array. Функция
`patch_data(file, marker, data, max_size)` изменяет его на месте: находит marker,
записывает `data` с padding `#` до `max_size` и проверяет, что размер файла не
изменился. Ничего не перемещается, не сжимается, не шифруется и не генерируется:
layout файла до и после одинаков, меняются только зарезервированные data bytes.
Изменяемые области содержат прозрачный **текст** identity/config, а не code:

- `PACKAGED_BUILD_NAME` - marker `###NotPackaged###`, array на 128 bytes. Строка identity package/build; каждый runtime variant изменяет собственную, чтобы `IsPackaged()` и build name отражали package.
- `INTERNAL_CONFIG` - markers `###InternalConfig###...` / `###InternalConfigEnd###`, фиксированная движком ёмкость 10000 bytes. Подключаемые проекты не могут менять размер baked bootstrap config blob.
- Embedded resources - ёмкость `FO_EMBEDDED_DATA_CAPACITY` (200000).

`package.py` также переписывает PE PDB path (`patch_pe_pdb_path`) и tokens
`$PLACEHOLDER$` Android Gradle.

> **Примечание для антивирусов:** большая placeholder region, изменяемая после
> сборки, внешне напоминает packer/dropper stub, но здесь это только прозрачный
> текст config/identity в зарезервированных data arrays: code не создаётся, не
> расшифровывается и не выполняется. Это документировано, чтобы release engineer
> или AV reviewer мог подтвердить безопасность. Не делайте ёмкости больше
> необходимого и добавляйте packaging step в allowlist. Любое product-specific
> antivirus exception фиксируйте в release-документации встраивающего проекта.

## Packaging: подпись кода Windows

Упакованные Windows binaries можно подписать во время release, чтобы повысить
доверие antivirus/SmartScreen к client. Self-update flow загружает и запускает
runtime DLL, поэтому неподписанный client является главным heuristic trigger.
Signing **выключен по умолчанию**, текущее поведение - unsigned, и не привязан к
конкретному tool:

- Задайте `Packaging.CodeSigningHook` в main config проекта как **исполняемый script** на packaging host.
- Во время `finalize_output`, до любого шага Zip/Tar/Wix/Raw, `package.py` вызывает `<hook> <absolute-pe-path>` один раз для **каждого `*.exe`/`*.dll`** в package tree: launcher executables, включая OpenGL variant, runtime DLL и client-runtime **update payloads**. Подпись после patch покрывает финальные bytes; охват всего дерева подписывает archives, MSI и updater payloads.
- Hook обязан завершаться с `0`; ненулевой код **останавливает package**, поэтому release, для которого запросили signing, не выйдет unsigned. Hook должен запускаться непосредственно на host: shebang и `chmod +x` в Linux либо `.cmd`/`.bat`/`.exe` в Windows.
- Hook владеет tool, certificate, timestamp URL и **secrets**. Храните passwords/tokens в environment variables, но не в repository или main config. Всегда используйте **timestamp** RFC-3161, чтобы signatures сохранялись после окончания cert.

Для публично доверенных signatures соблюдайте актуальные требования CA и
provider к хранению ключей; не предполагайте, что обычный `.pfx` на диске
допустим. Hook остаётся provider-neutral. Примеры форм интеграции:

```bash
# 1) Azure Trusted Signing on a Windows host through signtool and the provider dlib.
#    Azure credentials come from environment variables (AZURE_* / managed identity).
#    sign_windows.cmd  (args: %1 = file)
#    signtool sign /v /fd SHA256 /tr http://timestamp.acs.microsoft.com /td SHA256 ^
#      /dlib "%TRUSTED_SIGNING_DLIB%" /dmdf "%TRUSTED_SIGNING_METADATA_JSON%" "%~1"

# 2) SSL.com eSigner CodeSignTool on a Linux packaging host (Java). Credentials/TOTP come from env.
#    sign_windows.sh  (args: $1 = file)
#    CodeSignTool sign -username="$ESIGNER_USER" -password="$ESIGNER_PASS" \
#      -totp_secret="$ESIGNER_TOTP" -input_file_path="$1" -override

# 3) osslsigncode on Linux with a PKCS#11 cloud-HSM certificate or hardware token.
#    sign_windows.sh  (args: $1 = file)
#    tmp="$(mktemp)"; osslsigncode sign -pkcs11module "$PKCS11_MODULE" -key "$PKCS11_KEY_ID" \
#      -certs "$CERT_PEM" -h sha256 -n "Example Game" -i https://example.com \
#      -ts http://timestamp.digicert.com -in "$1" -out "$tmp" && mv -f "$tmp" "$1"
```

Для Android signing остаётся в Gradle, как описано в Android workflow выше;
этот hook предназначен только для Windows.

## Форматирование исходного кода

`buildtools.py format-source` форматирует дерево `Source/` Engine, включая
`.fos`, с помощью clang-format. `discover_clang_format()` разрешает binary в
таком порядке: override `FO_CLANG_FORMAT`, если задан, затем
`clang-format-20`/`clang-format` из `PATH`; разрешённый binary должен сообщать
major version 20. Затем BuildTools восстанавливает nullable и named-argument
forms AngelScript, которые clang-format разбирает как C++. Полный контракт и
граница встраивающего проекта описаны в
[руководстве по стилю AngelScript и рефакторингу](../Docs/ru/how-to/scripting/style-and-refactoring.md).

## Документация pipeline

Актуальное руководство по staged CMake pipeline находится в
[конвейер BuildTools](../Docs/ru/reference/cmake-and-buildtools/pipeline.md).
