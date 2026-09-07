---
layout: default
title: Сопровождение документации
locale: ru
document_id: documentation-maintenance
permalink: /Docs/ru/contributing/documentation/
---

<!-- docs-translation: {"document_id":"documentation-maintenance","locale":"ru","source_path":"Docs/en/contributing/documentation/index.md","source_sha256":"46b5925efb42b329b3d8d78989a5d7de5b60529273394d4466b0d7584466c2c7"} -->

# Сопровождение документации

> Документация движка. Эта страница объясняет, как сохранять документацию FOnline привязанной к исходному коду, удобной для навигации и отделённой от содержимого проектов, использующих движок.

## Назначение

Используйте эту страницу при добавлении, проверке и реорганизации документации движка. Это рабочее руководство дополняет машиночитаемый [манифест документации](../../../documentation-manifest.json), [бэклог документации](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationBacklog.md), [шаблон исследования](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationResearchTemplate.md), [отчёт о проверке](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationVerificationReport.md), [руководство по публикации сайта](site-publication.md), [индекс документации](../../index.md) и [точку входа для ИИ-сопровождения](../../../../AGENTS.md).

## Проверенные исходные пути

- `../AGENTS.md`
- `README.md`
- `Docs/en/index.md`
- `Docs/ru/index.md`
- `Docs/README.md` (legacy-маршрут)
- `Docs/_meta/DocumentationBacklog.md`
- `Docs/_meta/DocumentationExpansionPlan.md`
- `Docs/_meta/DocumentationResearchTemplate.md`
- `Docs/_meta/DocumentationVerificationReport.md`
- `Docs/documentation-manifest.json`
- `Docs/generated/api.json`
- `Docs/generated/api/*.md`
- `Docs/contract-change-dispositions.json`
- `Docs/generated/source-inventory.json`
- `Docs/generated/cli.json`
- `Docs/en/reference/buildtools/*.md`
- `Docs/ru/reference/buildtools/*.md`
- `Docs/generated/cli/*.md` (legacy-маршруты)
- `Docs/generated/helper-cli.json`
- `Docs/en/reference/helper-cli/*.md`
- `Docs/ru/reference/helper-cli/*.md`
- `Docs/generated/helper-cli/*.md` (legacy-маршруты)
- `Docs/generated/cmake.json`
- `Docs/en/reference/cmake/*.md`
- `Docs/ru/reference/cmake/*.md`
- `Docs/generated/cmake/*.md` (legacy-маршруты)
- `Docs/generated/native-extension.json`
- `Docs/generated/native-extension/*.md`
- `Docs/generated/prototype-format.json`
- `Docs/generated/prototype-format/*.md`
- `Docs/generated/map-format.json`
- `Docs/generated/map-format/*.md`
- `Docs/generated/model-format.json`
- `Docs/generated/model-format/*.md`
- `Docs/generated/text-format.json`
- `Docs/generated/text-format/*.md`
- `Docs/generated/effect-format.json`
- `Docs/generated/effect-format/*.md`
- `Docs/generated/image-format.json`
- `Docs/generated/image-format/*.md`
- `Docs/generated/particle-format.json`
- `Docs/ru/reference/particle-format/*.md`
- `Docs/generated/font-format.json`
- `Docs/ru/reference/font-format/*.md`
- `Docs/generated/audio.json`
- `Docs/en/reference/audio/*.md`
- `Docs/generated/video.json`
- `Docs/en/reference/video/*.md`
- `Docs/generated/gui-runtime.json`
- `Docs/generated/gui-runtime/*.md`
- `BuildTools/AiControlProtocol.json`
- `BuildTools/ai_control_client.py`
- `BuildTools/docs_ai_control_protocol.py`
- `BuildTools/tests/test_ai_control_protocol.py`
- `BuildTools/tests/test_docs_ai_control_protocol.py`
- `Examples/AiControlSample/`
- `Docs/generated/ai-control-protocol.json`
- `Docs/generated/ai-control-protocol/*.md`
- `Docs/generated/package.json`
- `Docs/generated/package/*.md`
- `Examples/PublicRepositories.json`
- `Examples/PublicRepositoryTemplate/`
- `BuildTools/docs_examples.py`
- `BuildTools/tests/test_docs_examples.py`
- `Docs/generated/public-examples.json`
- `Docs/generated/public-examples/*.md`
- `BuildTools/SupportMatrix.json`
- `BuildTools/docs_support_matrix.py`
- `BuildTools/tests/test_docs_support_matrix.py`
- `Docs/generated/support-matrix.json`
- `Docs/generated/support-matrix/*.md`
- `BuildTools/DocumentationDiagrams.json`
- `BuildTools/docs_diagrams.py`
- `BuildTools/tests/test_docs_diagrams.py`
- `Docs/generated/diagrams.json`
- `Docs/assets/diagrams/*.svg`
- `BuildTools/DocumentationScreenshots.json`
- `BuildTools/docs_screenshots.py`
- `BuildTools/tests/test_docs_screenshots.py`
- `Docs/generated/screenshots.json`
- `Docs/assets/screenshots/*.png`
- `BuildTools/SnippetPolicy.json`
- `BuildTools/docs_snippets.py`
- `BuildTools/tests/test_docs_snippets.py`
- `Docs/generated/snippets.json`
- `Docs/translation-glossary.json`
- `BuildTools/docs_localization.py`
- `BuildTools/tests/test_docs_localization.py`
- `Docs/generated/translation-status.json`
- `Docs/description-translations.ru.json`
- `BuildTools/docs_description_translations.py`
- `BuildTools/tests/test_docs_description_translations.py`
- `Docs/generated/description-translation-status.json`
- `Docs/ai-evaluation.json`
- `BuildTools/docs_ai_eval.py`
- `BuildTools/tests/test_docs_ai_eval.py`
- `Docs/generated/ai-evaluation-report.json`
- `BuildTools/docs_ai_delivery.py`
- `BuildTools/tests/test_docs_ai_delivery.py`
- `BuildTools/docs_site.py`
- `BuildTools/docs_site_artifact.py`
- `BuildTools/tests/test_docs_site.py`
- `BuildTools/tests/test_docs_site_layout.py`
- `BuildTools/tests/test_docs_site_artifact.py`
- `BuildTools/docs-browser/package.json`
- `BuildTools/docs-browser/package-lock.json`
- `BuildTools/docs-browser/audit.mjs`
- `BuildTools/tests/test_docs_browser.py`
- `BuildTools/web/default-index.html`
- `BuildTools/web/simple-web-server.py`
- `_data/docs-site.json`
- `assets/docs-search.json`
- `assets/docs-search.ru.json`
- `Docs/generated/document-routes.json`
- `llms.txt`
- `llms-full.txt`
- `docs-manifest.json`
- `BuildTools/docs_api.py`
- `BuildTools/docs_api_diff.py`
- `BuildTools/docs_contract_diff.py`
- `BuildTools/docs_public_api.py`
- `Docs/ru/reference/public-contract/index.md`, его EN-источник и legacy-маршрут `PUBLIC_API.md`
- `BuildTools/ExternalProjectEvidence.json`
- `BuildTools/docs_external_evidence.py`
- `BuildTools/tests/test_docs_external_evidence.py`
- `BuildTools/gameplay_test_runner.py`
- `BuildTools/tests/test_gameplay_test_runner.py`
- `BuildTools/tests/test_docs_gameplay_testing.py`
- `Docs/generated/external-project-evidence.json`
- `Docs/generated/external-project-evidence/index.md`
- `BuildTools/docs_reference.py`
- `BuildTools/docs_metadata.py`
- `BuildTools/docs_inventory.py`
- `BuildTools/docs_cli.py`
- `BuildTools/docs_helper_cli.py`
- `BuildTools/docs_native_extension.py`
- `BuildTools/docs_prototype_format.py`
- `BuildTools/docs_map_format.py`
- `BuildTools/docs_model_format.py`
- `BuildTools/docs_text_format.py`
- `BuildTools/docs_effect_format.py`
- `BuildTools/docs_image_format.py`
- `BuildTools/docs_particle_format.py`
- `BuildTools/docs_font_format.py`
- `BuildTools/docs_audio.py`
- `BuildTools/docs_video.py`
- `BuildTools/docs_gui_runtime.py`
- `BuildTools/docs_package.py`
- `BuildTools/docs_validate.py`
- `BuildTools/tests/test_docs_api.py`
- `BuildTools/tests/test_docs_api_diff.py`
- `BuildTools/tests/test_docs_contract_diff.py`
- `BuildTools/tests/test_docs_public_api.py`
- `BuildTools/tests/test_docs_reference.py`
- `BuildTools/tests/test_docs_metadata.py`
- `BuildTools/tests/test_docs_inventory.py`
- `BuildTools/tests/test_docs_cli.py`
- `BuildTools/tests/test_docs_helper_cli.py`
- `BuildTools/tests/test_docs_native_extension.py`
- `BuildTools/tests/test_docs_prototype_format.py`
- `BuildTools/tests/test_docs_map_format.py`
- `BuildTools/tests/test_docs_model_format.py`
- `BuildTools/tests/test_docs_text_format.py`
- `BuildTools/tests/test_docs_effect_format.py`
- `BuildTools/tests/test_docs_image_format.py`
- `BuildTools/tests/test_docs_particle_format.py`
- `BuildTools/tests/test_docs_font_format.py`
- `BuildTools/tests/test_docs_audio.py`
- `BuildTools/tests/test_docs_video.py`
- `BuildTools/tests/test_docs_gui_runtime.py`
- `BuildTools/tests/validate_native_extension_interface.cmake`
- `BuildTools/tests/test_docs_package.py`
- `BuildTools/tests/validate_package_interface.cmake`
- `BuildTools/tests/test_docs_validate.py`
- `.github/workflows/validate.yml`
- `_config.yml`, `Gemfile`, `.ruby-version` и `CNAME`
- репрезентативные документы подсистем в `Docs/`, проверенные по исходному коду

## Правила владения документацией

Документация движка должна описывать повторно используемое поведение движка:

- структуру исходного кода и архитектуру;
- точки входа приложений и инструментов сборки;
- поведение runtime, сущностей, сети, хранения, клиента, сервера и frontend;
- скриптинг, генерируемые метаданные, nullability и экспорт native-методов;
- bakers, Mapper, редакторы и общие механизмы инструментов;
- сборку и отладку платформ;
- маршрутизацию тестов и проверок.

Документация подключаемого проекта должна отвечать за:

- конкретный игровой контент, баланс, квесты, тексты, карты, фракции и политику релизов;
- игровые скрипты и native-расширения проекта;
- точные имена бинарных файлов и presets, если они не приведены только как примеры;
- генерируемые результаты продукта и downstream pipelines.

Документация движка не должна опираться на скрипты, тесты, инструменты, CI jobs или сгенерированные артефакты подключаемого проекта как на нормативное доказательство поведения движка. Если повторно используемый helper проверки достаточно важен для ссылки из документа движка, он должен находиться в репозитории движка. Проектный helper следует цитировать только в документации этого проекта.

Документация движка находится в `Engine/Docs/`. Не поддерживайте параллельные объяснения поведения движка в документации подключаемого проекта: ведите оттуда на страницу движка, оставляя только проектные wrappers, команды и правила.

Markdown-ссылки документации движка должны разрешаться внутри checkout движка. Не используйте пути родительского проекта даже в ненормативных примерах: укажите стабильную HTTPS-ссылку на tagged public example repository или опишите ответственность проекта обычным текстом, пока такого примера нет.

Каждая сопровождаемая Markdown-страница классифицирована в `Docs/documentation-manifest.json`. Манифест задаёт стабильный ID, аудиторию, тип Diataxis, видимость, область перевода, владельца домена, lifecycle state, место миграции и исходные пути. Там же определены rolling/current version channel, отложенная политика release snapshots, canonical и planned locales, явные README locale pairs и стратегия миграции маршрутов. Добавление, перемещение, удаление, смена цели или перевод страницы требуют изменения манифеста в том же наборе правок.

### Владельцы и требования к review

Манифест также задаёт единый review contract для каждого владельца домена. Изменение документации требует primary owner страницы или структурированного контракта, evidence этого владельца и всех co-reviews, вызванных изменённой границей. `localization` владеет parity локалей документации и проверкой носителем языка; `content-data` по-прежнему владеет форматом текста движка и механикой authored data. Build/release, runtime, scripting, content, frontend, networking, tooling, platform, quality, localization и documentation остаются разными обязанностями, даже если сейчас их выполняет один maintainer.

[Инвентарь внешних проектных доказательств и их продвижения](https://github.com/cvet/fonline/blob/master/Docs/generated/external-project-evidence/index.md) является проверяемым внутренним discovery ledger для Last Frontier и TLA. Каждая запись должна указывать точный snapshot source, disposition, priority, Engine или project target, primary owner, required reviews и promotion gate. Наличие записи само по себе не делает внешний проект нормативным: утверждение `promoted` заново выводится из исходного кода и тестов Engine, `boundary-owned` оставляет конкретную реализацию вне Engine, `promotion-candidate` фиксирует недостающие повторно используемые артефакты, а `project-owned` запрещает выдумывать контракт Engine. Обновляйте и проверяйте по исходникам этот ledger, если evidence любого проекта меняет решение о promotion или выявляет новую общую задачу; сам ledger исключён из публичного сайта и AI delivery.

## Стандартный процесс работы над группой документов

1. Выберите связную группу из [бэклога документации](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationBacklog.md).
2. Изучите исходные пути из бэклога и связанные тесты и build-файлы.
3. Напишите или обновите owning doc и раздел `Source paths inspected`.
4. Отдавайте приоритет отношениям исходного кода и границам владения, а не длинным перечням API-мелочей.
5. Добавляйте validation checklist в глубокие документы подсистем.
6. При изменении генерируемой поверхности проверяйте owning structured contract. Для native API обновляйте `///@ ApiContract` и используйте `BuildTools/docs_api_diff.py`; для project-facing CMake меняйте `BuildTools/cmake/ProjectInterface.json`; для main BuildTools CLI сохраняйте `create_parser()` авторитетным; для helper CLI обновляйте `BuildTools/HelperCliInterface.json`; для native extensions - `BuildTools/NativeExtensionInterface.json`. Изменения prototype, map, model, text, effect, image, particle, font, audio, video, GUI runtime и [AiControl](../../how-to/ai-control-protocol.md) требуют соответствующего `BuildTools/*Interface.json`, owning guide, генератора, focused tests и реального bake/runtime review подключаемого проекта. Изменения model animation также затрагивают [Model Animation](../../how-to/content/model-animation.md), sprite offsets и movement phase - [Sprite Root Motion](../../how-to/content/sprite-root-motion.md), package grammar - `BuildTools/PackageInterface.json`, а public example portfolio - `Examples/PublicRepositories.json` и [PublicExampleRepositories.md](../../../PublicExampleRepositories.md). Перегенерируйте все затронутые runtime models, сравните все восемнадцать runtime domains через `BuildTools/docs_contract_diff.py`, обновите root contract index через `BuildTools/docs_public_api.py` и заполните dispositions из [управления изменениями контрактов](../contract-change-management.md) для baseline-public и model-contract breaks. Project-authored remote calls остаются ответственностью проекта: bake обеих сторон и их каталог выполняются там.
   Для текущей 3D-подсистемы совладельцами двух bakers считаются `ModelSourceLoader`, `ModelAnimationConverter`, `ModelAnimationData`, `ModelMeshData`, `ModelManager`, `ModelInformation`, `ModelInstance` и `ModelAnimation`. Изменение parser, source, compatibility, mesh/rig wire, Ozz runtime или ownership требует обновления обоих model guides и structured contract, focused model/Ozz native suites, force rebake проекта, чистого следующего incremental bake и визуальной проверки pose/composition.
7. Добавьте или обновите запись в `Docs/documentation-manifest.json`, сохраняйте stable ID и locale target авторитетными и назначьте каждую public current human top-level page ровно одной группе `site_delivery.navigation`. Сначала регенерируйте source-owned diagrams через `BuildTools/docs_diagrams.py`, затем переснимайте triggered screenshots и обновляйте `Docs/generated/screenshots.json` через `BuildTools/docs_screenshots.py`. После этого обновляйте `Docs/generated/snippets.json` через `BuildTools/docs_snippets.py`. После восемнадцати source models и references регенерируйте канонические EN/RU индексы публичных контрактов и корневой legacy-маршрут. Затем обновляйте translation status; каждая существующая русская страница должна содержать новый normalized English hash. Далее `BuildTools/docs_site.py` создаёт `_data/docs-site.json`, оба search indexes и `Docs/generated/document-routes.json`. При изменении evaluation ownership/evidence или English search обновляйте `Docs/generated/ai-evaluation-report.json` через `BuildTools/docs_ai_eval.py`. В конце `BuildTools/docs_ai_delivery.py` создаёт `llms.txt`, `llms-full.txt` и `docs-manifest.json`. Public manifest хеширует diagram, screenshot, snippet, site и evaluation data; эти файлы нельзя редактировать вручную.
8. При добавлении новой пользовательской страницы обновите [индекс документации](../../index.md).
9. Повышайте статус в бэклоге только после semantic source review, а не после одной проверки ссылок.
10. Добавьте датированный раздел в [отчёт о проверке](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationVerificationReport.md) с scope, sources, fixes и checks.
11. Выполните `python BuildTools/docs_validate.py` и не оставляйте staged files, если владелец явно не попросил stage или commit.

### Перемещение публичного документа

Не считайте простое переименование файла достаточной миграцией маршрута:

1. Сохраните stable document ID на новой canonical page.
2. Переместите canonical content в английский target из манифеста и добавляйте matching Russian target только после review перевода.
3. Оставьте старый Markdown path как короткий durable pointer на canonical page, чтобы сохранить legacy URL в GitHub и Jekyll.
4. Отметьте старую запись как replacement/route alias, а новую сделайте единственным non-`replace` owner целевого пути.
5. Перегенерируйте `Docs/generated/document-routes.json`; старый маршрут должен появиться в `legacy_redirects` и указывать на ожидаемый canonical document ID.
6. Добавляйте в navigation/search только canonical page. Legacy pointer не является вторым searchable owner.
7. Требуйте `locale: en` / `locale: ru` front matter для новой пары, проверяйте stable-ID language switch и отсутствие чужой локали в каждом search index.
8. Перед удалением временного migration state выполните focused localization, site, AI-delivery, standalone validation, Jekyll artifact и browser locale-interaction checks.

Текущий маршрут остаётся canonical, пока весь набор правок не принят. Planned path в route catalog не разрешает удалять старый файл.

## Согласование при обновлении ревизии

Получение или смена ревизии движка является событием документации, а не только Git-операцией. Каждый входящий commit может изменить контракт, даже если в нём уже есть документация. Maintainer, выполняющий update, отвечает за reconciliation в том же worktree.

Перед обновлением:

1. Запишите текущий Engine SHA и целевую branch/ref.
2. Сохраните dirty worktree в именованном stash или отдельном worktree, включая untracked generated documentation, и не удаляйте safety copy до успешной проверки.
3. Сохраните текущую generated JSON model в ignored `Workspace/`, если её нет в committed baseline.

После fast-forward/rebase:

```bash
git log --oneline <old-engine-sha>..<new-engine-sha>
git diff --name-status <old-engine-sha>..<new-engine-sha>
git diff --stat <old-engine-sha>..<new-engine-sha>
```

Затем согласуйте каждую изменённую поверхность:

1. Читайте входящий исходный код и тесты, а не только commit subjects или prose.
2. Найдите owning page через [индекс документации](../../index.md) и поля `sources` в [манифесте документации](../../../documentation-manifest.json).
3. Сохраняйте полезную входящую документацию, но сразу исправляйте stale paths, project dependencies, неподтверждённые утверждения и отсутствующий validation evidence.
4. Обновляйте owning page и cross-links в том же worktree. Workaround или тест подключаемого проекта не является нормативным доказательством Engine; reusable proof должно находиться в этом репозитории.
5. Запишите точный SHA range, изменения контракта, generated delta, затронутые документы и проверки в [отчёте о проверке](https://github.com/cvet/fonline/blob/master/Docs/_meta/DocumentationVerificationReport.md).

Триггеры генерируемых поверхностей:

| Входящее изменение | Обязательное согласование |
|---|---|
| `BuildTools/codegen.py`, native metadata annotations, `Source/Scripting/` или `Source/Common/Settings.inc` | Перегенерировать native API model и Markdown; применить `docs_api_diff.py` и aggregate contract diff. |
| `BuildTools/cmake/ProjectInterface.json` или project-facing CMake implementation | Перегенерировать и проверить `Docs/generated/cmake.json`, канонические английские страницы в `Docs/en/reference/cmake/` и legacy-указатели в `Docs/generated/cmake/`; в том же изменении обновить проверенное русское зеркало и его source hash. Запустить `validate_project_interface.cmake`, focused CMake documentation test, localization checks и site validation. При необходимости обновить [ProjectDependencies.md](../../../ProjectDependencies.md), minimal fixture и проектную configure/build проверку. |
| `BuildTools/buildtools.py::create_parser()` или CLI help/default/choice | Через `docs_cli.py` перегенерировать и проверить `Docs/generated/cli.json`, канонические английские страницы в `Docs/en/reference/buildtools/` и legacy-указатели в `Docs/generated/cli/`; в том же изменении обновить проверенное русское зеркало и его source hash. Запустить focused CLI documentation test, localization checks и site validation. |
| Helper `create_parser()` или `BuildTools/HelperCliInterface.json` | Через `docs_helper_cli.py` перегенерировать и проверить `Docs/generated/helper-cli.json`, канонические английские страницы в `Docs/en/reference/helper-cli/` и legacy-указатели в `Docs/generated/helper-cli/`; в том же изменении обновить проверенное русское зеркало и его source hash. Запустить focused helper CLI test, localization checks и site validation, проверить owner/audience/invocation contract. |
| Prototype parser, metadata, text properties или `Baking.ProtoFileExtensions` | Обновить [PrototypeFormat.md](../../../PrototypeFormat.md), structured model, focused tests и bake проекта. |
| `MapLoader`, `MapBaker`, Mapper load/save, ownership или materialization | Обновить [формат карт](../../how-to/content/map-format.md), model/reference, Engine map tests и bake проекта. |
| Model bakers/loaders/converters, `.fo3d`, FBX/OBJ, layers, links, materials, animations или `FO_MODEL_*` | Обновить [формат моделей](../../how-to/content/model-format.md), [Model Animation](../../how-to/content/model-animation.md), model contract, native tests, force/incremental bake и визуальную проверку. |
| `.fotxt`, language normalization, prototype `$Text`, text methods или inline colors | Обновить [Текст и локализация](../../how-to/content/text-and-localization.md), model/reference, tests, bake и visible language switch. |
| `.fofx`, `EffectBaker`, render state/resources, runtime bindings или `FO_EFFECT_*` | Обновить [Формат эффектов](../../how-to/content/effect-format.md), model/reference, tests, bake и каждый affected backend/profile. |
| Image formats, FOFRM, `ImageBaker`, sprite records/factories, atlas или caches | Обновить [Форматы изображений и спрайтов](../../how-to/content/image-format.md), model/reference, tests, bake и визуальные dimensions/alpha/directions/cadence/hit masks. |
| Particle macros, SPARK/Effekseer sources/baking/rendering, Mapper tools, caches или model links | Обновить [формат и исполнение частиц](../../how-to/content/particle-format.md), model/reference, tests, bake и все enabled backends/integration paths. |
| `MapperEngine`, штатные меню/окна/controls/hotkeys/history/layout Mapper, mapper-side exports, headless capture карты, TGA/atlas readback или свидетельства видимого окна | Обновить [интерактивное руководство по Mapper](../../how-to/tools/mapper-interactive.md) и [инструменты Mapper](../../how-to/tools/mapper.md), запустить `test_docs_mapper_tools.py` и проверить затронутый интерактивный или headless-путь на fixture движка. При изменении UI, захвата, fixture или recorded trigger заново снять точный screenshot, обновить provenance, собрать Jekyll и проверить desktop/mobile; изменения map format или particles также следуют их owning rows. |
| `AnimationViewer`, `ParticleViewer`, их hosts/targets/packages, Mapper embedding или settings | Обновить [руководство по просмотру анимации и частиц](../../how-to/tools/animation-particle-viewers.md), focused test, affected binaries и visible workflow; также выполнить model/particle routes. |
| Font formats, raw-copy, `FontManager`, slots/flags, scale, measurement, wrapping или colors | Обновить [форматы шрифтов и компоновку текста](../../how-to/content/font-format.md), model/reference, tests, bake и visible text rendering. |
| Sound indexing/decoding, playback methods, audio settings/mixing или raw-copy | Обновить [Audio.md](../../../Audio.md), model/reference, tests, bake и audible validation на каждой заявленной платформе. |
| Ogg/Theora, fullscreen/embedded video, queue/input/music/drawing или raw-copy OGV | Обновить [Video.md](../../../Video.md), model/reference, tests, bake и visible first-frame/motion/completion/cleanup validation. |
| CoreScripts GUI types/API/lifecycle/layout/input или native dispatch | Обновить [GUI Runtime](../../how-to/runtime/gui.md), model/reference, tests и видимую проектную проверку; project formats/generators не переносить в Engine. |
| Package interface, `DefinePackage`, `Packages.cmake`, `package.py` или package behavior | Обновить [Packaging and Release](../../how-to/release/packaging.md), при необходимости [Support Matrix](../../reference/platforms/support-matrix.md), package model/tests/fixtures и re-audit signing, secrets, provenance, acceptance и rollback. |
| Native build configurations или symbol flags, `IsRunInDebugger`, `BreakIntoDebugger`, capture/resolution стека, exception/crash handlers, `FO_SELFTEST_CRASH`, Natvis/NatJMC, `Script.Debugger*`, endpoint/protocol отладчика AngelScript или адаптер VS Code | Обновить [нативную отладку и отладку AngelScript](../../troubleshooting/debugging.md) и её английский источник; при изменении файлов или ревизий обновить точное project evidence; выполнить `test_docs_debugging.py`, stack/exception и затронутые runtime tests, typecheck/build адаптера или live-endpoint gate, когда применимо, и статические/живые launch checks проекта. Сохранять loopback bind, отличать symbols от debug semantics, не переносить privacy ownership crash artifacts в Engine и не выводить live capability из mock controls или статического профиля. |
| Pin/preparation Emscripten, Web CMake flags, Web package/shell/server, canvas/clipboard/IDBFS/main-loop behavior, выбор WebSocket, Web native-updater capability, support label, hosting/security contract или project Web evidence | Обновить [сборку, упаковку и отладку в браузере](../../how-to/platforms/web-debugging.md), а при изменении границы также Packaging, Security, Support Matrix, Networking или Client Updater; выполнить `test_docs_web_debugging.py`, package/security/support tests, затронутый Web build, fresh bake/package, inspection HTTP artifacts/headers и применимые строки browser/release acceptance. Build-only evidence нельзя повышать до browser или production-deployment qualification. |
| Android platform/ABI mappings, SDK/NDK/API pins, package settings, Gradle/manifest/SDL templates, `FOnlineActivity`, resource staging, ADB endpoint/install/launch/log behavior, Android native-updater capability, support labels или project Android evidence | Обновить [сборку, упаковку и отладку на Android](../../how-to/platforms/android-debugging.md), а при изменении границы также Packaging, Security, Support Matrix или Client Updater; выполнить `test_docs_android_debugging.py`, package/security/support tests, затронутый Android native build, fresh bake и Gradle assembly, inspection APK, install/update, cold/warm launch и применимые строки device acceptance. Build-only evidence нельзя повышать до APK, device, release или store qualification. |
| Settings substitution/redaction, secret tokens, `ConfigBaker`, signing или CI credentials | Обновить [Security and Secrets](../../how-to/release/security-and-secrets.md), [Project Configuration](../../how-to/build/project-configuration.md), focused security tests и synthetic-secret audit. |
| Database backends, commit/reconnect/panic/oplog, persistence lifecycle, migration или backup/restore | Обновить [Persistence](../../explanation/persistence/), [Backup and Recovery](../../how-to/release/backup-and-recovery.md), tests и isolated semantic restore с RPO/RTO evidence. |
| Public example manifest/template/scaffold, visibility/pins или compatibility | Обновить [публичные репозитории с примерами](../../how-to/build/public-example-repositories.md), model/tests и rematerialized clean candidates в pinned/current modes. |
| Validation profiles, platform matrices/detection, runtime smoke или artifacts | Обновить support matrix source и [Support Matrix](../../reference/platforms/support-matrix.md), generator и affected validation target. |
| Gameplay runner/schema/harness/process smoke | Обновить [Gameplay and Integration Testing](../../how-to/testing/gameplay-and-integration.md), helper CLI и example manifest; выполнить focused tests и реальный baked smoke. |
| Tracy configurations/version/instrumentation | Обновить [Profiling](../../how-to/quality/profiling.md), focused test, affected profile build и isolated captures. |
| Canonical English prose, locale target, glossary или Russian translation | Обновить существующую пару, сохранить fences, hash и locale links; регенерировать localization, site, artifact и browser checks. |
| Обращённый к читателю текст в generated model, `Docs/description-translations.ru.json` или Russian generated reference | Сохранить canonical JSON и stable IDs, обновить перевод по стабильному локатору и точный source hash, перегенерировать владеющие model/pages и `description-translation-status.json`, выполнить focused test каталога и тест владеющего генератора. |
| Diagram manifest, owning doc, provenance, alt/caption или CSS | Обновить source manifest и prose, сгенерировать SVG/catalog, выполнить test, Jekyll build и visual review. |
| Screenshot manifest, UI/fixture, image, environment, interactions или trigger | Точно воспроизвести fixture, переснять без косметической обработки, обновить catalog, выполнить tests, Jekyll build и desktop/mobile review. |
| Fenced block или `BuildTools/SnippetPolicy.json` | Объявить language/harness в [Documentation Snippet Validation](snippets.md), обновить report, выполнить external parser и semantic owner checks. |
| Export methods, native tests или settings declarations | Перегенерировать `Docs/generated/source-inventory.json`. |
| Inventoried Markdown, ownership/state/target/source, publication/version/locale policy | Обновить manifest; затем snippets, site, AI evaluation и AI delivery. Не обходить context budget усечением. |
| Public title/path/state, README pair, site navigation/routing/search, Jekyll layout или assets | Обновить site data/search/routes, source/layout/artifact/browser tests и полный browser audit; не снижать gates. |
| AI task set, evidence, search ranking или source ref | Обновить [AI Documentation Evaluation](ai-evaluation.md), report и focused test; не добавлять нерелевантные keywords и не снижать threshold. |
| Metadata baker или remote-call format/runtime | Обновить [Remote Calls](../../reference/scripting/remote-calls.md); проект должен rebake обе стороны и свой catalog. |
| Module init, callbacks, `Yield`, scheduling/synchronization или teardown | Обновить [Script Lifecycle and Concurrency](../../how-to/scripting/lifecycle-and-concurrency.md) и narrow tests. |
| Порядок `.fos`, side macros, ownership/style, formatter, mutable globals, attributed calls, generated script или refactoring | Обновить [Стиль AngelScript и рефакторинг](../../how-to/scripting/style-and-refactoring.md) и английский оригинал, запустить focused test и владеющий formatter, выполнить compile без warnings и behavior test; project style остаётся внешним. |
| Model animation tokens/durations/aliases/metadata/methods | Обновить [Model Animation](../../how-to/content/model-animation.md), focused tests, API/reference при необходимости и bake проекта. |
| `NextX`/`NextY`, baked offsets или movement render phase | Обновить [Sprite Root Motion](../../how-to/content/sprite-root-motion.md), tests, bake и visible movement validation. |
| Build, package, platform, runtime, persistence, networking или pointer/nullability behavior | Обновить owning source-grounded page и narrow behavior/test path из неё. |

После каждого триггера генерируемой поверхности выполняйте `docs_contract_diff.py` против сохранённых старых models или выбранной Git base. Нулевой native API delta не закрывает изменения CMake, CLI, package, helper CLI, native extension и форматных/runtime contracts.

Update не завершён, пока generator сообщает stale output, входящее поведение не имеет documentation disposition, остаются conflict markers или safety stash является единственной копией нерешённой работы. Удаляйте safety stash только после финальных checks и подтверждения пустого staged area.

## Значения статусов бэклога

- `planned` - тема определена, исследование не начато.
- `researching` - идёт проверка исходного кода.
- `drafted` - первый документ существует, semantic validation не завершён.
- `verified` - страница проверена по текущему исходному коду, post-edit mechanical checks прошли.

Не оставляйте progress только в чате. Завершённая или blocked группа должна быть записана в backlog/report.

## Проверка ссылок и путей

Как минимум проверяйте:

- manifest coverage, ownership metadata и declared source paths;
- Markdown links и anchors во всех inventoried docs;
- что resolved local links остаются внутри Engine root;
- существование backticked source/build/doc paths;
- stale alternate-layout terms из старых snapshots;
- test inventory coverage при изменении [Testing](../testing/);
- `git diff --check`;
- staged area и working-tree status.

Выполните полный standalone gate из корня Engine. Test discovery не позволяет молча пропустить новый `test_docs_*.py`:

```bash
python -m unittest discover -s BuildTools/tests -p "test_docs_*.py"
python BuildTools/tests/test_gameplay_test_runner.py
python BuildTools/tests/test_minimal_multiplayer_package.py
python BuildTools/tests/test_ai_control_protocol.py
python BuildTools/tests/test_package_security.py
python BuildTools/tests/test_angelscript_cmake.py
cmake -P BuildTools/tests/validate_project_interface.cmake
cmake -P BuildTools/tests/validate_package_interface.cmake
cmake -P BuildTools/tests/validate_native_extension_interface.cmake
python BuildTools/docs_snippets.py --check --external
python BuildTools/docs_validate.py
```

Jobs `Validate documentation` и `Parse documentation snippets` в `.github/workflows/validate.yml` являются авторитетной развёрткой CI: они явно запускают каждый focused test и generator check, затем классифицируют изменения контрактов относительно base revision. Сохраняйте их и aggregate local route поведенчески эквивалентными. Подключаемый проект и native build им не нужны.

Изменения rendered output также должны следовать [руководству по публикации сайта](site-publication.md). При доступном pinned Ruby/Bundler/Node environment выполните `bundle exec jekyll build --trace`, `python BuildTools/docs_site_artifact.py --site-dir _site` и pinned browser audit. Каждый pull request получает GitHub Pages-compatible `_site` artifact и отдельные static/browser validation reports от job `Build documentation site`.

Планируемые будущие документы описывайте обычным текстом, если checker явно их не исключает; не оформляйте отсутствующие страницы как существующие code paths или links.

## Правила написания с опорой на исходный код

- Начинайте с назначения и маршрута для читателя.
- Добавляйте `Source paths inspected` в документы подсистем.
- Ведите на соседние документы вместо дублирования деталей.
- Указывайте точные пути исходного кода для ownership.
- Для cross-project evidence используйте tagged public example URLs; не включайте локальный checkout проекта в процедуру Engine.
- Не обещайте неподдерживаемые workflows; описывайте то, что подтверждают текущие source/test/build wiring.
- При переносе ownership обновляйте связанные документы.

## Примечания для ИИ-сопровождения

[AGENTS.md](../../../../AGENTS.md) является точкой входа ИИ-maintainer. Он ведёт к human docs и фиксирует repository conventions, включая запрет commit/push без явной просьбы. Сохраняйте его кратким и навигационным, а подробные процедуры помещайте в `Docs/`. Root `llms.txt`, `llms-full.txt` и `docs-manifest.json` являются generated retrieval routes для внешних агентов; `_data/docs-site.json`, оба locale search indexes и `Docs/generated/document-routes.json` образуют соответствующую human navigation/search/routing projection. Все семь артефактов должны выводиться из одного manifest/corpus.

Будущий ИИ-agent, продолжающий roadmap, должен сначала восстановить контекст по git status, backlog и verification report. Контекст чата вторичен относительно состояния репозитория.

## Контрольный список проверки

1. Новые документы классифицированы в `Docs/documentation-manifest.json` и связаны из [индекса документации](../../index.md) и при необходимости [AGENTS.md](../../../../AGENTS.md).
2. Статус бэклога соответствует реальному source validation state.
3. Verification report содержит каждую promoted group.
4. Для API changes есть base-revision report, а required public dispositions проходят [управление изменениями контрактов](../contract-change-management.md).
5. AI delivery и human site delivery регенерированы, focused tests/checks проходят.
6. `python BuildTools/tests/test_docs_validate.py` и `python BuildTools/docs_validate.py` проходят.
7. Link/path/test/stale-term checks проходят после обновления report.
8. `git diff --cached --name-only` пуст, если staging явно не запрошен.
9. Финальный report перечисляет изменённые файлы и подтверждает отсутствие commit/push, если их не просили.
