---
layout: default
title: Сгенерированный API и метаданные
locale: ru
document_id: generated-api-metadata
permalink: /Docs/ru/reference/metadata/
---

<!-- docs-translation: {"document_id":"generated-api-metadata","locale":"ru","source_path":"Docs/en/reference/metadata/index.md","source_sha256":"f313dd930058b6133b03abf824274274cd0e51426e9e44a35185b5469d3e7778"} -->

# Сгенерированный API и метаданные

Этот документ описывает потоки генерации кода и регистрации метаданных движка. Используйте его при изменении generated source, metadata annotations, определений свойств и видимых скриптам API contracts.

Практический порядок регенерации configure-time codegen, скриптов, запечённых ресурсов, метаданных, документации, сайта и AI-артефактов приведён в разделе [Работа со сгенерированным содержимым](../../how-to/build/generated-content.md).

## Модель владения

Движок владеет переиспользуемым механизмом metadata/codegen. Встраиваемый проект передаёт конфигурацию проекта, дополнительные источники метаданных, общие заголовки и script/content inputs через опции CMake и project files.

Generated files являются build artifacts. Документируйте исходные annotations, templates, generator inputs и validation flow; не считайте generated output вручную сопровождаемым исходным кодом движка.

## Проверенные исходные пути

- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/EngineSources.cmake`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/helpers/State.cmake`
- `BuildTools/buildtools.py`
- `BuildTools/codegen.py`
- `BuildTools/docs_api.py`
- `BuildTools/docs_api_diff.py`
- `BuildTools/docs_contract_diff.py`
- `BuildTools/docs_cli.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/docs_helper_cli.py`
- `BuildTools/NativeExtensionInterface.json`
- `BuildTools/docs_native_extension.py`
- `BuildTools/PackageInterface.json`
- `BuildTools/package.py`
- `BuildTools/docs_package.py`
- `BuildTools/docs_examples.py`
- `BuildTools/tests/test_docs_examples.py`
- `BuildTools/docs_cmake.py`
- `BuildTools/docs_reference.py`
- `BuildTools/ModelFormatInterface.json`
- `BuildTools/docs_model_format.py`
- `BuildTools/TextFormatInterface.json`
- `BuildTools/docs_text_format.py`
- `BuildTools/EffectFormatInterface.json`
- `BuildTools/docs_effect_format.py`
- `BuildTools/ImageFormatInterface.json`
- `BuildTools/docs_image_format.py`
- `BuildTools/ParticleFormatInterface.json`
- `BuildTools/docs_particle_format.py`
- `BuildTools/FontFormatInterface.json`
- `BuildTools/docs_font_format.py`
- `BuildTools/AudioInterface.json`
- `BuildTools/docs_audio.py`
- `BuildTools/tests/test_docs_audio.py`
- `BuildTools/VideoInterface.json`
- `BuildTools/docs_video.py`
- `BuildTools/tests/test_docs_video.py`
- `BuildTools/GuiRuntimeInterface.json`
- `BuildTools/docs_gui_runtime.py`
- `BuildTools/tests/test_docs_gui_runtime.py`
- `BuildTools/AiControlProtocol.json`
- `BuildTools/docs_ai_control_protocol.py`
- `BuildTools/tests/test_docs_ai_control_protocol.py`
- `BuildTools/docs_metadata.py`
- `BuildTools/docs_public_api.py`
- `BuildTools/tests/test_docs_public_api.py`
- `BuildTools/docs_ai_delivery.py`
- `BuildTools/tests/test_docs_ai_delivery.py`
- `BuildTools/docs_ai_eval.py`
- `BuildTools/tests/test_docs_ai_eval.py`
- `BuildTools/docs_snippets.py`
- `BuildTools/tests/test_docs_snippets.py`
- `BuildTools/SupportMatrix.json`
- `BuildTools/docs_support_matrix.py`
- `BuildTools/tests/test_docs_support_matrix.py`
- `BuildTools/docs_localization.py`
- `BuildTools/tests/test_docs_localization.py`
- `BuildTools/ExternalProjectEvidence.json`
- `BuildTools/docs_external_evidence.py`
- `BuildTools/tests/test_docs_external_evidence.py`
- `BuildTools/DocumentationDiagrams.json`
- `BuildTools/docs_diagrams.py`
- `BuildTools/tests/test_docs_diagrams.py`
- `BuildTools/DocumentationScreenshots.json`
- `BuildTools/docs_screenshots.py`
- `BuildTools/tests/test_docs_screenshots.py`
- `BuildTools/docs_site.py`
- `BuildTools/docs_site_artifact.py`
- `BuildTools/tests/test_docs_site.py`
- `BuildTools/tests/test_docs_site_layout.py`
- `BuildTools/tests/test_docs_site_artifact.py`
- `BuildTools/tests/test_docs_browser.py`
- `BuildTools/tests/test_docs_api.py`
- `BuildTools/tests/test_docs_api_diff.py`
- `BuildTools/tests/test_docs_contract_diff.py`
- `BuildTools/tests/test_docs_cli.py`
- `BuildTools/tests/test_docs_helper_cli.py`
- `BuildTools/tests/test_docs_native_extension.py`
- `BuildTools/tests/validate_native_extension_interface.cmake`
- `BuildTools/tests/test_docs_package.py`
- `BuildTools/tests/validate_package_interface.cmake`
- `BuildTools/tests/test_docs_cmake.py`
- `BuildTools/tests/test_docs_reference.py`
- `BuildTools/tests/test_docs_model_format.py`
- `BuildTools/tests/test_docs_text_format.py`
- `BuildTools/tests/test_docs_effect_format.py`
- `BuildTools/tests/test_docs_image_format.py`
- `BuildTools/tests/test_docs_particle_format.py`
- `BuildTools/tests/test_docs_font_format.py`
- `BuildTools/tests/test_docs_metadata.py`
- `Docs/generated/api.json`
- `Docs/en/reference/script-api/*.md`
- `Docs/ru/reference/script-api/*.md`
- `Docs/generated/api/*.md` (legacy routes)
- `BuildTools/cmake/ProjectInterface.json`
- `BuildTools/tests/validate_project_interface.cmake`
- `Docs/generated/cmake.json`
- `Docs/en/reference/cmake/*.md`
- `Docs/ru/reference/cmake/*.md`
- `Docs/generated/cmake/*.md` (legacy routes)
- `Docs/generated/cli.json`
- `Docs/en/reference/buildtools/*.md`
- `Docs/ru/reference/buildtools/*.md`
- `Docs/generated/cli/*.md` (legacy routes)
- `Docs/generated/helper-cli.json`
- `Docs/en/reference/helper-cli/*.md`
- `Docs/ru/reference/helper-cli/*.md`
- `Docs/generated/helper-cli/*.md` (legacy routes)
- `Docs/generated/native-extension.json`
- `Docs/generated/native-extension/*.md`
- `Docs/generated/prototype-format.json`
- `Docs/generated/map-format.json`
- `Docs/generated/package.json`
- `Docs/generated/package/*.md`
- `Docs/generated/model-format.json`
- `Docs/generated/model-format/*.md`
- `Docs/generated/text-format.json`
- `Docs/generated/text-format/*.md`
- `Docs/generated/effect-format.json`
- `Docs/generated/effect-format/*.md`
- `Docs/generated/image-format.json`
- `Docs/generated/image-format/*.md`
- `Docs/generated/particle-format.json`
- `Docs/en/reference/particle-format/*.md`
- `Docs/generated/font-format.json`
- `Docs/en/reference/font-format/*.md`
- `Docs/generated/audio.json`
- `Docs/en/reference/audio/*.md`
- `Docs/generated/audio/*.md` (legacy routes)
- `Docs/generated/video.json`
- `Docs/en/reference/video/*.md`
- `Docs/generated/video/*.md` (legacy routes)
- `Docs/generated/gui-runtime.json`
- `Docs/en/reference/gui-runtime/*.md`
- `Docs/generated/gui-runtime/*.md` (legacy routes)
- `Docs/generated/ai-control-protocol.json`
- `Docs/en/reference/ai-control-protocol/*.md`
- `Examples/PublicRepositories.json`
- `Examples/PublicRepositoryTemplate/`
- `Docs/generated/public-examples.json`
- `Docs/generated/public-examples/*.md`
- `Docs/en/reference/public-contract/index.md`
- `Docs/ru/reference/public-contract/index.md`
- `PUBLIC_API.md` (legacy route)
- `Docs/contract-change-dispositions.json`
- `llms.txt`
- `llms-full.txt`
- `docs-manifest.json`
- `Docs/ai-evaluation.json`
- `Docs/generated/ai-evaluation-report.json`
- `BuildTools/SnippetPolicy.json`
- `Docs/generated/snippets.json`
- `Docs/generated/support-matrix.json`
- `Docs/ru/reference/platforms/generated-matrix.md`
- `Docs/generated/translation-status.json`
- `Docs/generated/external-project-evidence.json`
- `Docs/generated/external-project-evidence/index.md`
- `Docs/generated/diagrams.json`
- `Docs/assets/diagrams/*.svg`
- `Docs/generated/screenshots.json`
- `Docs/assets/screenshots/*.png`
- `_data/docs-site.json`
- `assets/docs-search.json`
- `assets/docs-search.ru.json`
- `Docs/generated/document-routes.json`
- `Source/Common/MetadataRegistration.h`
- `Source/Common/MetadataRegistration.cpp`
- `Source/Common/MetadataRegistration.template.cpp`
- `Source/Common/GenericCode.template.cpp`
- `Source/Common/Properties.h`
- `Source/Common/Properties.cpp`
- `Source/Common/Entity.h`
- `Source/Common/Entity.cpp`
- `Source/Tools/MetadataBaker.h`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tests/Test_EngineMetadata.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `Source/Tests/Test_Properties.cpp`
- `PUBLIC_API.md`

## Стадия codegen в CMake

`BuildTools/cmake/stages/Codegen.cmake` формирует команду генератора и список outputs.

Основные аргументы команды:

- `-maincfg` — основной config встраиваемого проекта (`FO_MAIN_CONFIG`).
- `-buildhash` — текущий build hash.
- `-genoutput` — каталог generated output, сейчас `GeneratedSource` внутри CMake binary dir.
- `-devname` / `-nicename` — значения идентичности проекта.
- `-embedded` — ёмкость embedded data (`FO_EMBEDDED_DATA_CAPACITY`).
- `-meta` — записи источников metadata из `FO_SOURCE_META_FILES` и `FO_MONO_SOURCE`.
- `-commonheader` — дополнительные общие headers из `FO_ADDED_COMMON_HEADERS`.
- `-enginedefine` — повторяемый конфигурационный макрос значения/формы движка `NAME=VALUE` (`FO_GEOMETRY`, `FO_MAP_*`, `FO_EFFECT_*`, `FO_MODEL_*`, `FO_USE_NAMESPACE`, `FO_NO_*`, `FO_MAIN_CONFIG`, ...). На этапе configure он разрешается в literal и записывается в `EngineConfig.gen.h`, а не передаётся компилятору как `-D`. Feature/backend toggles (`FO_ENABLE_3D`, `FO_*_SCRIPTING`, `FO_*_PARTICLES`) и per-config `FO_DEBUG` остаются на стороне компилятора: они исключают целые files/headers до подключения любого engine header.

Стадия создаёт обычный и принудительный command target генерации кода и добавляет `CodeGeneration` в `FO_GEN_DEPENDENCIES`.

`InternalConfig.gen.inc` резервирует фиксированную движком patch area размером
10000 bytes. Подключаемый проект не может менять её размер; перед записью
bootstrap config `package.py` определяет точную ёмкость по markers в generated
binary.

## Результаты генерации

`Codegen.cmake` объявляет generated outputs в `GeneratedSource/`, включая:

- `CodeGenTouch`
- `EngineConfig.gen.h` — единый macro-only header, подключаемый в начале `Source/Essentials/BasicCore.h`. Он содержит конфигурационные макросы движка и строковые build/version macros `FO_BUILD_HASH` / `FO_DEV_NAME` / `FO_NICE_NAME` / `FO_COMPATIBILITY_VERSION` / `FO_GIT_BRANCH`. Заменяет прежний `Version-Include.h`.
- `EmbeddedResources.gen.inc`
- `InternalConfig.gen.inc`
- `MetadataRegistration-Server.gen.cpp`
- `MetadataRegistration-Client.gen.cpp`
- `MetadataRegistration-Mapper.gen.cpp`
- `MetadataRegistration-ServerStub.gen.cpp`
- `MetadataRegistration-ClientStub.gen.cpp`
- `MetadataRegistration-MapperStub.gen.cpp`
- `GenericCode-Common.gen.cpp`

Имена файлов помогают понимать build flow, но изменения обычно должны вноситься в templates, annotations, источники metadata или generator scripts, а не в generated output.

Resource bakers также создают runtime metadata за пределами этого набора codegen outputs. В частности, `ModelInfoBaker` записывает `ModelAnimationInfo.foinfo`, который `BaseEngine` регистрирует для `Game.GetModelAnimDuration`; private layout и source-owned semantics анимации описаны в разделе [Анимация моделей](../../how-to/content/model-animation.md).

## Каноническая модель API документации

[generated/api.json](../../../generated/api.json) является детерминированной machine-readable моделью native codegen surface движка. `BuildTools/docs_api.py` не разбирает объявления C++ независимо: он запускает read-only metadata session через `BuildTools/codegen.py`, затем сериализует те же typed tag objects, которые используются при генерации bindings и регистрации metadata.

Сейчас модель покрывает:

- exported enums и enum values;
- value types и layout fields;
- reference types, fields и methods;
- entities, properties, native script methods и exported events;
- settings из блоков `///@ ExportSettings`;
- native migration rules;
- source-authored объявления stability/lifecycle из тегов `///@ ApiContract`;
- допустимые targets, имена hooks и виды migration rules парсера codegen.

Каждая адресуемая запись имеет стабильный `family_id`, уникальный `id`, kind, runtime sides, receiver там, где он применим, нормализованную script-facing signature, flags, description, source path/line, stability, поля since/deprecation, связанные examples и explicit/default provenance контракта. Аргументы содержат defaults, nullability и by-reference state. Properties и settings содержат mutability; settings также указывают, совпадает ли имя с default policy маскирования command line `Common.SecretSettingTokens`. Поле намеренно называется `command_line_redacted_by_default`: оно не классифицирует credentials, не обещает маскировку каждого похожего на credential имени и не учитывает изменённый token list.

Settings, объявленные в одном блоке `///@ ExportSettings`, в текущей модели разделяют source line этой annotation. Стабильной identity остаётся имя setting; точные per-macro lines можно позднее добавить во владеющий parser codegen без изменения IDs.

Generated `summary` сообщает число symbols по kind и stability, explicit declarations контрактов и затронутые symbols, default-internal coverage, покрытие descriptions, отсутствующий provenance и вклад source files. Эти числа являются generated quality signals; не копируйте их в вручную сопровождаемый публичный текст.

Одиночные объявления используют family ID напрямую. Семейства overloads добавляют к `id` детерминированный hash сигнатуры, сохраняя unhashed `family_id` для группировки. Поэтому изменение сигнатуры non-overloaded symbol сохраняет identity; изменение overloaded signature выглядит как удаление/добавление до появления explicit source-authored IDs.

Неклассифицированные записи получают определённый ADR уровень stability `internal`. Доступность через generated bindings не повышает symbol до `stable` или `experimental`.

### Описания символов во владении исходников

Обращённые к читателю описания остаются рядом с export metadata, определяющими каждый symbol:

- обычный соседний комментарий или inline-комментарий export-тега описывает экспортируемый type, entity, method, event, property или setting;
- fields и methods внутри блоков `///@ ExportRefType` используют соседние или inline-комментарии своих members;
- явные enum values используют `///@ EnumValueDoc <Enum> <Value> // <description>` после своей декларации `ExportEnum`, когда в C++ initializer нет подходящей поверхности документации;
- fields layout value type используют `///@ ValueFieldDoc <Type> <Field> // <description>` после своего тега `ExportValueType`. Эта форма работает и для alias или strong type, у которых script layout не имеет соответствующего field declaration рядом с alias.
- сгенерированные enum values от `GameProperty` до `LocationProperty` наследуют точное описание и source location своего owning `ExportProperty`; каждое сгенерированное значение `None` явно означает, что идентификатор свойства не выбран.
- 23 enum wrapper `ImGui_*` сопоставляют каждый inline alias Dear ImGui с закреплённым `ThirdParty/imgui/imgui.h`. Значения с upstream-комментарием наследуют его текст и точную source line; alias `ImGui_StyleVar` сопоставляется с содержательным комментарием соответствующего field `ImGuiStyle`. Для нулевого, составного, исправленного или иного значения без самостоятельного описания обязателен явный fallback `EnumValueDoc`.
- 103 физических значения `KeyCode` сопоставляются с точными строками mapping `SDL_SCANCODE_*` в `Source/Frontend/Application.cpp`; sentinel отсутствия клавиши и синтетическое UTF-8 событие `Text` остаются явными records `EnumValueDoc` в header декларации.

`EnumValueDoc` и `ValueFieldDoc` отклоняют неизвестные types, неизвестные members, дубликаты и пустые descriptions. Resolver Dear ImGui также отклоняет отсутствующий или malformed alias wrapper и любое значение без пригодного vendored prose или явного fallback; resolver key code отклоняет объявленное значение без точного SDL mapping или явного описания synthetic value. Документация сохраняет точную source line owning tag, property, mapping, vendored value или field `ImGuiStyle` и исключена из runtime compatibility hash. Документация generated property enum присоединяется только после вычисления compatibility contribution, поэтому её изменение не влияет на client/server compatibility. Description объясняет текущее поведение; stability задаётся только отдельным exact, family или scope contract.

### Аннотации контрактов API

`///@ ApiContract` является documentation-only тегом, который разбирается той же metadata session `BuildTools/codegen.py`, что и runtime exports. Он намеренно исключён из client/server compatibility hash. Тег выбирает один точный `id` symbol, все текущие overloads с общим `family_id` либо полный закреплённый инвентарём scope native-codegen:

```cpp
///@ ApiContract script.method.common.Game.BreakIntoDebugger internal
///@ ApiContract scope:native-codegen experimental Since=2022.1.0.wip SymbolCount=2492 InventorySha256=8f70fbf7882f6b81ca6141efc8e3e0240eb97b0e8a9b36fe6be530345846f4ab
///@ ApiContract script.method.common.Game.LoadData experimental Since=0.4.0 Example=Docs/Examples/LoadData.md
///@ ApiContract script.method.common.Game.OldCall deprecated DeprecatedSince=0.5.0 Replacement=script.method.common.Game.NewCall Removal=1.0.0
```

Поддерживаемые labels и fields определены ADR 0002:

- `internal` не принимает lifecycle fields и фиксирует, что default status явно проверен.
- `stable` и `experimental` требуют `Since=<version>`.
- `scope:native-codegen` требует положительный `SymbolCount` и записанный строчными буквами 64-значный `InventorySha256` для стабильных ID, лексически отсортированных и соединённых переводом строки. Оба пина должны совпадать до применения метки scope.
- `deprecated` требует `DeprecatedSince=<version>`, существующий `Replacement=<id-or-family>` и `Removal=<target>`; optional `Since` записывает исходную версию появления, если она известна.
- `Example=<root-relative-path-or-http-url>` может повторяться. Local paths должны существовать и оставаться внутри корня движка.
- Обычный комментарий `///` непосредственно перед тегом становится notes контракта. Он не заменяет description symbol из export annotation.

Генерация отвергает неизвестные selectors, устаревшие пины scope, несколько scope declarations, пересекающиеся exact/family declarations, отсутствующие replacements, self-replacements, некорректные examples, повторяющиеся fields и неполные lifecycle metadata. Exact declaration может намеренно переопределять единственный scope. Provenance контракта хранится отдельно от provenance объявления, поэтому читатель различает `experimental (scope)`, точные классификации и неклассифицированное значение по умолчанию.

Текущая проверенная классификация применяет привязанный к ревизии статус `experimental` ко всем 2 472 символам native-codegen с версии `2022.1.0.wip`, а exact override вспомогательного метода разработки `Game.BreakIntoDebugger` сохраняет статус `internal`. Count и digest инвентаря scope требуют owner review при каждом будущем изменении набора символов. Широкие обещания `stable` и lifecycle-классификация `deprecated` остаются задачей release policy и owner review.

Добавление и изменение тегов `ApiContract` не должно менять runtime compatibility hash. Focused API tests сравнивают hashes с корректными contract metadata и без них.

Scope модели намеренно ограничен `engine-native-codegen`. Project-authored remote calls покрывает отдельное дополнение baked metadata ниже, потому что они принадлежат встраиваемому проекту, а не snapshot движка. Для CMake options/stages/helpers, основного BuildTools CLI, package declarations/payloads и helper-script CLIs существуют отдельные source-backed модели. Детали ABI native extensions требуют собственного structured source до закрытия полного exit gate публичного справочника.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_api.py
python BuildTools/docs_api.py --write
python BuildTools/docs_api.py --check
```

JSON хранится в репозитории для GitHub Pages, offline use и AI retrieval. Не редактируйте его вручную: CI и `BuildTools/docs_validate.py` отвергают stale output.

## Сгенерированный Markdown-справочник

[Русский справочник native script API](../../reference/script-api/index.md) является канонической human entry point этой локали; английская версия остаётся source-of-truth и доступна через переключатель локали, а `Docs/generated/api/*.md` сохраняется как durable legacy route. `BuildTools/docs_reference.py` читает только каноническую JSON model: он не разбирает C++, не придумывает отсутствующие descriptions и не ведёт параллельный inventory symbols. Generated pages делят текущую модель на:

- native script methods;
- entity properties;
- events движка;
- entities, enums, value types и reference types с их members;
- settings движка;
- native migration rules.

Каждый symbol находится под детерминированным HTML anchor и содержит unique ID, нормализованную signature, runtime sides, explicit/default API contract, provenance объявления и контракта, lifecycle fields, examples, применимые flags и source-authored description. Source links указывают на branch `master`, а каноническая модель сохраняет generated path/line provenance. Revision-pinned links остаются задачей publication и не являются неявной гарантией этого renderer.

Страница settings показывает `command_line_redacted_by_default` как результат command-line masking policy. Она не переименовывает поле в `sensitive` и не представляет его как классификацию credentials.

После JSON model регенерируйте и проверяйте Markdown layer:

```bash
python BuildTools/tests/test_docs_reference.py
python BuildTools/docs_reference.py --write
python BuildTools/docs_reference.py --check
```

Набор страниц и byte-for-byte output объявлены в [documentation-manifest.json](../../../documentation-manifest.json). Standalone validator и documentation CI отвергают отсутствующие, вручную изменённые и stale pages.

## Сгенерированный интерфейс проекта CMake

[Русский справочник CMake](../../reference/cmake/index.md) является human entry point CMake surface для встраиваемых проектов; английский source-of-truth доступен через переключатель локали. `BuildTools/cmake/ProjectInterface.json` является документационной моделью, а configure-time authority остаётся у `BuildTools/Init.cmake` и файлов стадий/helper-команд. Структурный CMake-тест сопоставляет имена options, порядок stages, entrypoints, hooks, объявления helpers и пути к исходникам, чтобы сгенерированная документация не расходилась с реализацией.

`BuildTools/docs_cmake.py` независимо проверяет manifest и создаёт [generated/cmake.json](../../../generated/cmake.json), канонические English pages для options, stages/hooks и выбранных helpers, а также durable pointers по прежним маршрутам `Docs/generated/cmake/*.md`. Каждая запись имеет стабильный ID `cmake.option.*`, `cmake.stage.*` или `cmake.helper.*`. Markdown output содержит defaults, required state, override precedence, signatures, допустимые roles, responsibilities и source links без повторного разбора синтаксиса CMake. Русские страницы являются reviewed translations с gates source hash и code-fence parity; английский generator их не перезаписывает.

Текущий scope равен `experimental`: модель фиксирует точный revision-pinned interface, но не объявляет versioned support line. Команды в `BuildTools/cmake/helpers/` не становятся публичными только из-за доступности; в documented surface входят лишь helpers из manifest. Grammar package declaration и command-line interfaces BuildTools являются отдельными domains, а aggregate contract gate сравнивает каждую модель по её stability policy.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_cmake.py
cmake -P BuildTools/tests/validate_project_interface.cmake
python BuildTools/docs_cmake.py --write
python BuildTools/docs_cmake.py --check
```

Структурный CMake test проверяет runtime-loaded форму stage/entrypoint/hook/helper. Focused Python tests и `BuildTools/docs_validate.py` проверяют schema rules, детерминированные IDs, source paths, escaped Markdown и byte-for-byte freshness.

## Сгенерированный справочник BuildTools CLI

[Русский справочник BuildTools CLI](../../reference/buildtools/index.md) является human entry point основной command line BuildTools; английский source-of-truth доступен через переключатель локали. `BuildTools/docs_cli.py` импортирует `BuildTools/buildtools.py`, вызывает ту же factory `create_parser()`, что и executable, и создаёт [generated/cli.json](../../../generated/cli.json), канонические English command pages и durable pointers по прежним маршрутам `Docs/generated/cli/*.md`. Второй список команд или parser Python source не сопровождается.

Каждая команда и argument получают стабильный ID `cli.buildtools.*`. Модель записывает positional/optional kind, action, cardinality, choices, defaults, type, parser description, usage, точный output `--help`, source и детерминированный contract digest. Отсутствующий текст argument явно помечается как parser documentation gap; descriptions должны находиться в `create_parser()`, чтобы executable help и generated reference улучшались вместе.

Текущий scope равен `internal`: модель даёт revision-pinned truth, но не обещает versioned CLI support line. Helper scripts и domain invocation/payload `package.py` имеют отдельные модели ниже. Все три модели участвуют в aggregate revision comparator.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_cli.py
python BuildTools/docs_cli.py --write
python BuildTools/docs_cli.py --check
```

Focused test сравнивает generated top-level help с executable `buildtools.py --help`, проверяет детерминированные IDs и Markdown escaping и доказывает обнаружение stale output в режимах write/check. `BuildTools/docs_validate.py` и documentation CI также отвергают отсутствующие или вручную изменённые CLI outputs.

## Сгенерированный справочник helper CLI

[Русский справочник helper CLI](../../reference/helper-cli/index.md) является human entry point executable helper scripts движка вне основных command lines BuildTools и package; английский source-of-truth доступен через переключатель локали. `BuildTools/HelperCliInterface.json` владеет ID helper, purpose, owner, audiences и invocation owner. `BuildTools/docs_helper_cli.py` импортирует каждую объявленную factory `create_parser()` и создаёт [generated/helper-cli.json](../../../generated/helper-cli.json), канонические English pages на основе точного help и durable pointers по прежним маршрутам `Docs/generated/helper-cli/*.md`.

Generator также сканирует `BuildTools/**/*.py` через AST Python. Новая top-level `create_parser()` должна входить в helper manifest либо быть явно исключена как принадлежащая другой канонической модели; иначе генерация завершается ошибкой. Так поверхности `buildtools.py` и `package.py` остаются раздельными, а новый executable helper не может обойти документацию.

Каждый helper, argument и subcommand имеет стабильный ID `helper-cli.*`. Текущая модель покрывает code generation, компиляцию Mono scripts, сбор и отчёты coverage, gameplay process tests, client протокола AiControl, проверку PE imports для Windows 7, управление Android device, packaged local web server и создание MSI. Она записывает точный 80-column help, parser type/default/choice/cardinality, source, ownership, audience и invocation context. Scope равен `internal` и revision-pinned; публикация точного синтаксиса не обещает cross-revision compatibility.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_helper_cli.py
python BuildTools/docs_helper_cli.py --write
python BuildTools/docs_helper_cli.py --check
```

Focused test вызывает каждый реальный helper и subcommand с `--help`, проверяет детерминированные output и IDs, доказывает enforcement AST inventory и обнаружение stale output. Parser help принадлежит executable factory, а текст о владении принадлежит manifest.

## Сгенерированный интерфейс native extensions

[Справочник native extensions](../../reference/native-extension/index.md) точно описывает roles project-native source, используемые текущими targets, поддерживаемые hooks движка, generated fallbacks и правила native bindings. `BuildTools/NativeExtensionInterface.json` является данными документации и проверки, а не runtime input; `BuildTools/docs_native_extension.py` и структурный CMake-тест сопоставляют его с текущим поведением CMake/codegen и создают [generated/native-extension.json](../../../generated/native-extension.json) вместе со страницами roles, hooks и bindings.

Каждая role, hook и binding rule имеет стабильный ID `native-extension.*`. Модель записывает role libraries/consumers, hook signatures/call sites/defaults, presence в compatibility hash и правила registration/namespace/pointer/dependency. Scope равен `experimental` и revision-pinned: документируется source-compatible использование на зафиксированной ревизии движка, но binary compatibility независимо собранных ревизий не обещается. Project implementations, SDKs, settings, persistence и packaging остаются во владении проекта.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_native_extension.py
cmake -P BuildTools/tests/validate_native_extension_interface.cmake
python BuildTools/docs_native_extension.py --write
python BuildTools/docs_native_extension.py --check
```

Затем принадлежащий движку [минимальный проект](../../../../Examples/MinimalProject/README.ru.md) доказывает один export `SERVER` и один optional hook через configure, codegen, bake, link и runtime. Правила authoring и lifecycle находятся в разделе [Native extensions](../../how-to/native-extensions.md).

## Сгенерированный справочник формата прототипов

[Справочник формата прототипов](../../reference/prototype-format/index.md) точно описывает выбор prototype input, формы sections, control directives, встроенные entity types/properties `HasProtos` и source-backed validation rules. `BuildTools/PrototypeFormatInterface.json` владеет reviewed grammar contract; `BuildTools/docs_prototype_format.py` проверяет anchors по `ProtoBaker`, parsing configuration/properties и source settings, затем выводит каталог built-in properties из live API metadata model.

Каждая section, directive, rule, entity type и property имеет стабильный ID `prototype-format.*`. Записи grammar/rules имеют уровень `experimental`, а revision-derived inventory entities/properties остаётся `internal`. Модель записывает applicability parser, side-specific skip behavior, исключение temporary/virtual, provenance и текущие defaults движка, не импортируя extensions, fixed types, custom properties, IDs или gameplay semantics конкретного проекта.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_prototype_format.py
python BuildTools/docs_prototype_format.py --write
python BuildTools/docs_prototype_format.py --check
```

Руководство по authoring, наследованию, migration и границе проекта находится в разделе [Формат прототипов](../../how-to/content/prototype-format.md). Встраиваемому проекту следует публиковать companion catalog из объединённых metadata движка и проекта и доказывать семантические комбинации полей реальной выпечкой и subsystem tests.

## Сгенерированный справочник формата карт

[Справочник формата карт](../../reference/map-format/index.md) точно описывает sections `.fomap`, placement directives, владение items, properties Map/Critter/Item, side-specific bake output, round trip Mapper и runtime materialization. `BuildTools/MapFormatInterface.json` владеет reviewed grammar и behavioral rules; `BuildTools/docs_map_format.py` проверяет live anchors loader/baker/mapper/runtime и выводит каталоги properties и `ItemOwnership` из текущей API model.

Каждая section, directive, ownership mode, rule и property имеет стабильный ID `map-format.*`. Grammar, ownership и behavior rules имеют уровень `experimental`, revision-derived property catalog остаётся `internal`. Project map IDs, custom metadata, catalogs, composition policy, quests, encounters и level-design rules находятся вне модели движка.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_map_format.py
python BuildTools/docs_map_format.py --write
python BuildTools/docs_map_format.py --check
```

Руководство по authoring и границе проекта находится в разделе [Формат карт](../../how-to/content/map-format.md). Встраиваемому проекту следует ссылаться на него ради переиспользуемой grammar, сохраняя конкретный каталог карт, графические kits, generators и gameplay validation в проектной документации.

## Сгенерированный справочник формата моделей

[Справочник формата моделей](../../reference/model-format/index.md) является human projection текущего parser `.fo3d`, mesh input, composition, animation adjacency и validation contract. `BuildTools/ModelFormatInterface.json` владеет стабильными records compile limits, assets, tokens и rules; `BuildTools/docs_model_format.py` сравнивает каждое допустимое spelling непосредственно с `ModelDescriptionParser::ParseToken`, проверяет source anchors и project-interface limits и создаёт [generated/model-format.json](../../../generated/model-format.json), семь канонических Markdown pages и durable legacy routes.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_model_format.py
python BuildTools/docs_model_format.py --write
python BuildTools/docs_model_format.py --check
```

Generated reference полностью покрывает текущую parser surface. [Формат моделей](../../how-to/content/model-format.md) остаётся human guide по ordering, state transitions, composition practices, действующим и удалённым tokens и границе владения между движком и проектом.

## Сгенерированный справочник текстового формата

[Справочник текстового формата](../../reference/text-format/index.md) является
human projection raw-синтаксиса `.fotxt`, упорядоченной нормализации языков,
prototype `$Text`, runtime lookup, цветовых tags renderer и validation.
`BuildTools/TextFormatInterface.json` владеет стабильными records rules и methods;
`BuildTools/docs_text_format.py` проверяет их live source anchors, выводит defaults
движка для `Baking.BakeLanguages` и `Client.Language`, сопоставляет пять prototype
output packs с `ProtoTextBaker` и создаёт [generated/text-format.json](../../../generated/text-format.json)
вместе с шестью Markdown pages.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_text_format.py
python BuildTools/docs_text_format.py --write
python BuildTools/docs_text_format.py --check
```

[Текст и локализация](../../how-to/content/text-and-localization.md) остаётся human guide
по authoring practices, bake-time fallback, runtime behavior при missing data и
границе project-owned каталогов packs и lexem formatters.

## Сгенерированный справочник формата эффектов

[Справочник формата эффектов](../../reference/effect-format/index.md) является human
projection sections `.fofx`, state pass/render, vertex layouts, встроенных samplers
и uniform buffers, descriptor conventions, backend artifacts, runtime cache identity,
script values и validation. `BuildTools/EffectFormatInterface.json` владеет
стабильными records limits, sections, options, resources, baking, runtime,
script methods и validation; `BuildTools/docs_effect_format.py` проверяет live
anchors baker/renderer/runtime, выводит текущие defaults лимитов CMake и создаёт
[generated/effect-format.json](../../../generated/effect-format.json) и семь Markdown pages.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_effect_format.py
python BuildTools/docs_effect_format.py --write
python BuildTools/docs_effect_format.py --check
```

[Формат эффектов](../../how-to/content/effect-format.md) остаётся human guide по
authoring, поведению backend/resources, path-only cache identity, lifetime
`ScriptValueBuf`, policy project overrides и видимой cross-backend validation.

## Сгенерированный справочник форматов изображений

[Справочник форматов изображений](../../reference/image-format/index.md) является
human projection двенадцати source extensions ImageBaker, полей и flattening
FOFRM, legacy filename selectors, private baked records, покрытия stock client
factory, sprite sheets, atlases, caches и validation. `BuildTools/ImageFormatInterface.json`
владеет стабильными records formats, fields, options, baking, runtime и validation.
`BuildTools/docs_image_format.py` проверяет их live anchors baker/client/tests,
выводит списки extensions baker и default runtime, проверяет игнорируемую границу
FOFRM Effect и создаёт [generated/image-format.json](../../../generated/image-format.json)
вместе с семью Markdown pages.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_image_format.py
python BuildTools/docs_image_format.py --write
python BuildTools/docs_image_format.py --check
```

[Форматы изображений и спрайтов](../../how-to/content/image-format.md) остаётся
human guide по выбору source, authoring FOFRM, legacy import, границам baking/runtime,
поведению atlas/cache, project policy, diagnostics и видимой validation.

## Сгенерированный справочник форматов частиц

[Справочник форматов частиц](../../reference/particle-format/index.md) является
human projection pipelines `.spark`/`.spk` и `.efkproj`/`.efk`, зарегистрированной
SPARK graph surface, поддерживаемого профиля Effekseer, обязательных baked bounds,
client caches/render paths, integrations и validation. `BuildTools/ParticleFormatInterface.json`
владеет стабильными records family, object, XML, renderer, tooling, runtime,
integration и validation. `BuildTools/docs_particle_format.py` выводит live registry,
local descriptor attributes, parity editor, backend extensions/options и defaults
движка, затем создаёт [generated/particle-format.json](../../../generated/particle-format.json)
и восемь Markdown pages.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_particle_format.py
python BuildTools/docs_particle_format.py --write
python BuildTools/docs_particle_format.py --check
```

[Форматы и runtime частиц](../../how-to/content/particle-format.md) остаётся human
guide по authoring, round trip editor, effects/textures, caches, выбору atlas/direct scene,
integration scripts/models, diagnostics и project-owned visual validation.

## Сгенерированный справочник форматов шрифтов

[Справочник форматов шрифтов](../../reference/font-format/index.md) является human
projection descriptors FOFNT и AngelCode BMFont, доставки ресурсов, font slots,
bind-time scale, измерения и переноса текста, rendering flags, inline colors,
поведения atlas/cache и validation. `BuildTools/FontFormatInterface.json` владеет
reviewed records formats, fields, binding, layout, rendering и validation.
`BuildTools/docs_font_format.py` выводит live registries extensions, parser keys,
binary constants, signed reads metrics, enums, scale guards, поведение atlas/cache
и evidence bundled descriptors, затем создаёт [generated/font-format.json](../../../generated/font-format.json)
и восемь Markdown pages.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_font_format.py
python BuildTools/docs_font_format.py --write
python BuildTools/docs_font_format.py --check
```

[Форматы шрифтов и layout текста](../../how-to/content/font-format.md) остаётся human
guide по authoring, binding, layout, diagnostics, project-owned policy slots/glyphs
и видимой validation.

## Сгенерированный справочник аудио

[Справочник аудио](../../reference/audio/index.md) точно отражает delivery,
decoding, streaming, mixing, repeat и playback formats WAV, ACM и Ogg Vorbis
в stock client. `BuildTools/AudioInterface.json` владеет стабильными records
formats, delivery, decoding, playback и validation. `BuildTools/docs_audio.py`
проверяет их по RawCopyBaker, ResourceManager, SoundManager, script methods,
settings и live source anchors, затем создаёт [generated/audio.json](../../../generated/audio.json),
шесть канонических English pages и durable pages по прежним маршрутам
`Docs/generated/audio/*.md`.

Модель имеет уровень `experimental` и является revision-pinned. Она намеренно
не включает каталог звуков игры, правила spatialization, music state machine,
mastering, licensing и evidence audible acceptance.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_audio.py
python BuildTools/docs_audio.py --write
python BuildTools/docs_audio.py --check
```

[Аудиоресурсы и воспроизведение](../../how-to/content/audio.md) остаётся human
guide по delivery, выбору authoring, diagnostics, границам проекта и audible validation.

## Сгенерированный справочник видео

[Справочник видео](../../reference/video/index.md) точно отражает текущий primitive
Ogg/Theora: raw-copy delivery, buffering ресурса целиком, decoding packets/headers/frames,
CPU conversion YCbCr-to-RGBA, fullscreen queues, interruption вводом, pairing
отдельной музыки, embedded `VideoPlayback` и границы visible validation.
`BuildTools/VideoInterface.json` владеет стабильными records; `BuildTools/docs_video.py`
проверяет их source anchors и создаёт [generated/video.json](../../../generated/video.json),
семь канонических English pages и durable legacy routes.

Модель имеет уровень `experimental` и является revision-pinned. Это не проектная
система синематиков: контракт не заявляет decoding audio из container, subtitles,
localization, story policy, streaming, DRM, accessibility acceptance или asset provenance.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_video.py
python BuildTools/docs_video.py --write
python BuildTools/docs_video.py --check
```

[Видеоресурсы и воспроизведение](../../how-to/content/video.md) остаётся human
guide по delivery, runtime use, diagnostics, project policy и visible acceptance.

## Сгенерированный справочник GUI runtime

[Справочник GUI runtime](../../reference/gui-runtime/index.md) точно отражает
переиспользуемую object model AngelScript GUI, screen API из
`Source/Scripting/AngelScript/CoreScripts/Gui.fos` и integration ввода.
`BuildTools/GuiRuntimeInterface.json` владеет стабильными records types, callbacks,
lifecycle, layout, rendering, input, integration и validation.
`BuildTools/docs_gui_runtime.py` проверяет declarations и behavioral anchors
по live scripts и native integration points, затем создаёт
[generated/gui-runtime.json](../../../generated/gui-runtime.json), семь канонических
English pages и durable legacy routes.

Модель имеет уровень `experimental` и является revision-pinned. Она явно
объявляет ноль принадлежащих движку declarative GUI formats: `.fogui`, generators,
screen catalogs, styles, fonts, images, gameplay presentation и accessibility
acceptance остаются обязанностями встраиваемого проекта.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_gui_runtime.py
python BuildTools/docs_gui_runtime.py --write
python BuildTools/docs_gui_runtime.py --check
```

[GUI runtime](../../how-to/runtime/gui.md) остаётся human guide по lifecycle,
project hooks, границам authoring, diagnostics и end-to-end validation.

## Сгенерированный интерфейс пакетов

[сгенерированный справочник пакетов](../packages/index.md) является human entry point для package declarations и payloads. `BuildTools/PackageInterface.json` моделирует текущие возможности `DefinePackage`/`package.py` для документации и проверки; packager этот manifest не читает. `BuildTools/docs_package.py` также вызывает executable `package.py::create_parser()` и создаёт [generated/package.json](../../../generated/package.json) вместе со страницами declaration, matrix, payload/artifact и CLI.

Модель присваивает стабильные IDs `package.*` конструкции `DefinePackage`, двум её clauses, per-binary modifier `POSTFIX`, шести targets, шести platforms, девятнадцати pack tokens, шести platform payloads и тринадцати arguments packager. Набор targets включает standalone packages `AnimationViewer` и `ParticleViewer`. Реализованные platforms/packs отделяются от explicit unsupported и placeholders, а комментарии не выдаются за поддержку. Runtime validation до output staging отвергает неизвестные или повторные packs/architectures, unsupported platforms, несовместимые комбинации target/platform/pack, отсутствие обязательного `NoRes` и списки только из modifiers. Windows matrix включает selectors `win32-win7` и `win64-win7` и описывает их canonical architecture с explicit postfix.

Scope package сейчас равен `internal` и revision-pinned. Он не импортирует package matrix или release policy проекта и исключает schema project configuration keys, provisioning secrets, точный состав ресурсов, deployment topology и работу внешних signing tools. Эти обязанности документирует встраиваемый проект.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_package.py
cmake -P BuildTools/tests/validate_package_interface.cmake
python BuildTools/docs_package.py --write
python BuildTools/docs_package.py --check
```

Focused Python test покрывает согласие manifest/parser, runtime validation packs, executable help, стабильные IDs, детерминированный escaped Markdown и обнаружение stale output. Структурный CMake test выполняет реальный macro `DefinePackage` и проверяет `CONFIG`, повторные `BINARY`, per-entry storage `POSTFIX` и изоляцию соседних entries. Standalone validator и documentation CI требуют обе проверки и byte-for-byte generated outputs.

## Сгенерированный справочник протокола AiControl

[Справочник протокола AiControl](../../reference/ai-control-protocol/index.md)
является human entry point experimental project-neutral envelope управления AI.
`BuildTools/AiControlProtocol.json` владеет 49 стабильными entries wire, methods,
errors, common commands, security, integration и validation.
`BuildTools/docs_ai_control_protocol.py` проверяет declarations по reference client
на standard library и runnable sample, затем создаёт
[generated/ai-control-protocol.json](../../../generated/ai-control-protocol.json)
и шесть проверяемых reference pages.

Модель намеренно исключает project observations, имена игровых actions,
administrator commands, readiness semantics, namespaces MCP и launch policy.
`Examples/AiControlSample` доказывает authorization, framing request/response,
bounded state commands/events, correlation accepted commands, asynchronous
completion, replacement observations и cursors событий, не притворяясь native
клиентом FOnline. Project evidence native/script/server authority и shipping build
остаётся обязательным по разделу [Протокол AiControl](../../how-to/ai-control-protocol.md).

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_ai_control_protocol.py
python BuildTools/tests/test_docs_ai_control_protocol.py
python Examples/AiControlSample/run_protocol_smoke.py
python BuildTools/docs_ai_control_protocol.py --write
python BuildTools/docs_ai_control_protocol.py --check
```

## Сгенерированный реестр публичных репозиториев-примеров

[сгенерированный реестр публичных примеров](../public-examples/index.md) является human projection внешнего portfolio примеров. `Examples/PublicRepositories.json` владеет стабильными repository IDs, ordering, dependencies, accountable roles, lifecycle state, обязательными checks/artifacts, policy точной фиксации Engine, compatibility lanes, asset policy и exit gates. `BuildTools/docs_examples.py` проверяет этот source и `Examples/PublicRepositoryTemplate`, затем создаёт [generated/public-examples.json](../../../generated/public-examples.json).

Тот же инструмент проверяет candidate external repository. В pinned mode он требует точного совпадения `example-repository.json`, committed gitlink `Engine/` и checked-out revision Engine. В current mode release gitlink сохраняется, а временно проверенная revision Engine показывается отдельно. Оба режима отвергают отсутствующие governance files, неразрешённые publication placeholders и неполный asset provenance.

Регенерация и проверка из корня движка:

```bash
python BuildTools/tests/test_docs_examples.py
python BuildTools/docs_examples.py --write
python BuildTools/docs_examples.py --check
```

Реестр является documentation/governance metadata, а не дополнительным runtime compatibility domain. Владение examples и authority публикации определены в разделе [Публичные репозитории-примеры](../../how-to/build/public-example-repositories.md) и [ADR-0005](../../contributing/decisions/0005-public-example-repository-ownership.md).

## Инвентарь фрагментов документации

`BuildTools/SnippetPolicy.json` объявляет полную карту поддерживаемых fence languages
и parser harnesses. `BuildTools/docs_snippets.py` сканирует каждый public/current/human
документ manifest, включая generated reference pages, и записывает
[generated/snippets.json](../../../generated/snippets.json). Для каждого fenced block
report содержит стабильного owner документа, heading/line, normalized content hash,
template status, contract, harness и result.

Normative blocks commands/source/config/data требуют 100-процентного parser coverage.
Обычные blocks `text` сохраняются как evidence и проверяются на transport-safe structure.
Gate `--external` вызывает Bash в parse-only mode и language parser PowerShell;
ни один harness не выполняет документированные команды. Lexical checks C-family
не заменяют владеющий compile, bake или smoke example, если текст заявляет semantic outcome.

При изменении fenced content или policy регенерируйте и проверяйте inventory
до localization/site/AI delivery:

```bash
python BuildTools/tests/test_docs_snippets.py
python BuildTools/docs_snippets.py --write --external
python BuildTools/docs_snippets.py --check --external
```

Правила authoring, templates, failures и semantic owners описаны в разделе
[Проверка фрагментов документации](../../contributing/documentation/snippets.md).

## Evidence и медиа-артефакты документации

`BuildTools/ExternalProjectEvidence.json` записывает точные revisions Last Frontier
и FOnline TLA, изученные ради переиспользуемых practices. Records классифицируют
каждую concern как promoted, promotion candidate, project-owned или boundary,
принадлежащую другому owner. `BuildTools/docs_external_evidence.py` проверяет
source paths, targets Engine, ownership, review roles и promotion gates, затем
создаёт [generated/external-project-evidence.json](https://github.com/cvet/fonline/blob/master/Docs/generated/external-project-evidence.json)
и internal audit page. Внешние проекты остаются discovery/compatibility evidence
и никогда не заменяют source, tests, manifests или Engine-owned examples.

`BuildTools/DocumentationDiagrams.json` и `BuildTools/DocumentationScreenshots.json`
являются reviewed inventories визуального evidence документации.
`BuildTools/docs_diagrams.py` создаёт детерминированные desktop/mobile SVG diagrams
из объявленных nodes и relationships. `BuildTools/docs_screenshots.py` проверяет
checked-in PNG paths, dimensions, source revision, capture command, owner, alt text
и freshness policy. Полученные JSON models позволяют сайту и CI проверять provenance,
не превращая binary media в runtime contract.

Регенерация и проверка этих артефактов из корня движка:

```bash
python BuildTools/tests/test_docs_external_evidence.py
python BuildTools/tests/test_docs_diagrams.py
python BuildTools/tests/test_docs_screenshots.py
python BuildTools/docs_external_evidence.py --write
python BuildTools/docs_diagrams.py --write
python BuildTools/docs_screenshots.py --write
```

Перед изменением claims о reusable practices обновите pinned external snapshots
и evidence record в том же change. Screenshot является только supporting evidence;
авторитет сохраняют владеющий source-backed reference и его executable validation.

## Сгенерированный статус поддержки и локализации

`BuildTools/SupportMatrix.json` является reviewed source текущих profiles host, target, architecture, compiler, application и evidence. `BuildTools/docs_support_matrix.py` проверяет указанные targets BuildTools и workflow lanes, затем создаёт [generated/support-matrix.json](../../../generated/support-matrix.json) и [сгенерированную матрицу поддержки](../platforms/generated-matrix.md). Модель отличает source capability от обязательных build, process smoke и qualification встраиваемого проекта или device; она не заявляет, что выполнена каждая комбинация targets.

`BuildTools/docs_localization.py` проецирует канонический inventory human documents, [translation-glossary.json](../../../translation-glossary.json), normalized source hashes и существующие locale counterparts в [generated/translation-status.json](../../../generated/translation-status.json). Существующая translation обязана содержать ожидаемые document ID, locale, source path/hash, byte-identical fenced code и internal links, сохраняющие язык. Отсутствующие counterparts показываются в pre-production migration и становятся ошибкой при `--enforce-complete`.

Регенерация и проверка из корня движка:

```bash
python BuildTools/docs_support_matrix.py --write
python BuildTools/docs_support_matrix.py --check
python BuildTools/docs_localization.py --write
python BuildTools/docs_localization.py --check
```

Эти модели описывают documentation evidence и freshness перевода. Они не добавляют runtime compatibility domains и не превращают CI build в обещание поддержки. Интерпретация и порядок обновления находятся в [матрице поддержки](../../reference/platforms/support-matrix.md) и [процессе перевода](../../contributing/documentation/translation.md).

## Доставка документации для AI

Machine-oriented entry layer создаётся из того же [documentation-manifest.json](../../../documentation-manifest.json), который владеет human pages. `BuildTools/docs_ai_delivery.py` не разбирает source движка и не изобретает вторую API model. Он проецирует reviewed document metadata и канонический Markdown в:

- root `llms.txt`, который маршрутизирует public current documents через source-ref-pinned clean Markdown URLs, связывает canonical HTML routes и перечисляет все канонические generated JSON models;
- root `llms-full.txt`, содержащий authored public current documents и generated reference indexes в строгом byte budget;
- root `docs-manifest.json`, публикующий rolling/current version channel, deferred state release snapshot, locale policy, public stable IDs, owner/state/disposition, source, clean Markdown, raw и canonical HTML URLs, source provenance, normalized content hashes и hashes generated artifacts.

[ai-evaluation.json](../../../ai-evaluation.json) является reviewed versioned source задач по architecture, scripting, content, debugging, migration и release. `BuildTools/docs_ai_eval.py` проверяет ownership задач и sentinels answer evidence, пропускает каждый retrieval query через тот же ranking contract `docs_site.search_documents`, что использует browser search, и записывает [generated/ai-evaluation-report.json](../../../generated/ai-evaluation-report.json). Детерминированный report доказывает только выбор route и текущее evidence; runs ответов model family остаются отдельным reviewed evidence по разделу [Оценка AI-документации](../../contributing/documentation/ai-evaluation.md).

Full-context output намеренно исключает generated detail pages. Их канонические JSON models точнее и компактнее несут полные inventories methods, types, properties, settings, CMake, CLI, native extensions, prototypes, maps, models, text, effects, images, particles, fonts, audio, video, GUI runtime, protocol AiControl, packages и public examples. Generated indexes остаются в bundle, чтобы agent мог выбрать правильные model и source.

После изменения manifest или inventoried Markdown регенерируйте и проверяйте из корня движка:

```bash
python BuildTools/tests/test_docs_ai_delivery.py
python BuildTools/tests/test_docs_ai_eval.py
python BuildTools/docs_site.py --write
python BuildTools/docs_ai_eval.py --write
python BuildTools/docs_ai_eval.py --check
python BuildTools/docs_ai_delivery.py --write
python BuildTools/docs_ai_delivery.py --check
```

Public files являются discovery/transport artifacts, а не owners контрактов. Stability API по-прежнему определяется source-owned metadata и ADR-0002; policy доставки документации и byte budget задаёт [ADR-0003](../../contributing/decisions/0003-manifest-backed-ai-documentation-delivery.md).

## Данные сайта документации

Human navigation сайта, search, identity version/locale и migration routes используют те же records manifest, не становясь generated API domains. `BuildTools/docs_site.py` разрешает стабильные document IDs в `_data/docs-site.json` для Jekyll/Liquid, токенизирует public current human Markdown в независимые ограниченные English и Russian indexes browser search и записывает `Docs/generated/document-routes.json` для текущих URLs, канонических future owners, доступных locale pairs и обязательных legacy redirects.

Navigation model требует точного покрытия top-level reader pages, оставляя generated detail pages за их generated indexes. Search включает detail pages, повышает вес titles/headings относительно body tokens, сохраняет technical identifiers и хранит только compact postings и result metadata. Он не копирует полные Markdown bodies в browser artifact и не создаёт hosted search contract.

После render репозитория Jekyll инструмент `BuildTools/docs_site_artifact.py` проверяет готовое дерево `_site` по route и artifact models. Этот post-build layer доказывает, что обещанные routes, доступные locale pages, static JSON/text/assets, canonical metadata, accessibility landmarks/names, search targets и publishable local links пережили processing Jekyll. Его JSON report является CI evidence, а не ещё одним checked-in generated reference.

Затем `BuildTools/docs-browser/audit.mjs` обслуживает точное дерево через ephemeral loopback port. Зафиксированный lock file Playwright Chromium посещает каждый route generated catalog в размерах 1440 x 1000 и 390 x 844, внедряет pinned engine axe-core для заявленных tags WCAG 2.2 A/AA и записывает findings runtime/resources, responsive layout, page overflow и accessibility. Отдельные interaction profiles доказывают skip navigation, modal search, persistence theme, status code copy, semantics mobile drawer, containment focus и restoration по Escape. JSON report и desktop/mobile screenshots являются CI evidence; ни один из них не является generated compatibility model движка.

Регенерируйте и проверяйте после изменения title/path/state/target публичной страницы, policy version/localization, policy navigation/routing или search:

```bash
python BuildTools/tests/test_docs_site.py
python BuildTools/tests/test_docs_site_layout.py
python BuildTools/tests/test_docs_site_artifact.py
python BuildTools/tests/test_docs_browser.py
python BuildTools/docs_site.py --write
python BuildTools/docs_site.py --check
python BuildTools/docs_site_artifact.py --site-dir _site
npm ci --prefix BuildTools/docs-browser
npx --prefix BuildTools/docs-browser playwright install chromium
npm --prefix BuildTools/docs-browser run audit
```

Эти artifacts описывают текущую revision документации и presentation routes. Они не участвуют в eighteen-domain compatibility diff движка и не повышают page, symbol или helper до stable public API. Владение navigation/search записано в [ADR-0004](../../contributing/decisions/0004-manifest-backed-site-navigation-search.md); rolling/release versioning, locale targets и durable route migration находятся в [ADR-0006](../../contributing/decisions/0006-documentation-version-locale-routing.md).

## Multi-domain diff и оформление изменений

`BuildTools/docs_contract_diff.py` сравнивает все восемнадцать канонических generated models с теми же paths в base revision. Native symbols делегируются `BuildTools/docs_api_diff.py` с сохранением behavior overload families и source-owned stability; entries CMake/CLI/package/helper CLI/native extension/prototype format/map format/model format/text format/effect format/image format/particle format/font format/audio/video/GUI runtime/protocol AiControl сравниваются по стабильным IDs.

Breaking changes используют baseline stability. Удалённая или изменённая entry уровня `stable`, `experimental` или `deprecated` не проходит `--enforce` без точной domain-bound записи в `Docs/contract-change-dispositions.json`; смена текущего label на `internal` не обходит gate. Изменения model source, model scope и model-level contract всегда требуют disposition. Изменения internal declarations остаются видимыми, не становясь случайными compatibility promises.

Aggregate diff записывает `Workspace/contract-diff.json` и `.md`; CI загружает их как artifacts, а не добавляет revision-pair reports на current-reference site. Полные local/CI workflow, classifications, per-domain digest binding, ledger schema v2, ограничения и checklist review находятся в разделе [Управление изменениями generated contracts](../../contributing/contract-change-management.md).

## Проектное дополнение remote calls

Remote calls объявляются в project files `.fos` и разбираются `Source/Tools/MetadataBaker.cpp`, поэтому параллельный source parser не должен добавлять их в `api.json`. После project bake инструмент `BuildTools/docs_metadata.py` строго декодирует authoritative outputs `Metadata.fometa-server` и `Metadata.fometa-client`, проверяет совпадение сигнатур и структурных пределов на обеих сторонах и создаёт project-owned каталог JSON/Markdown.

Из корня встраиваемого проекта:

```bash
python Engine/BuildTools/docs_metadata.py \
  --root . \
  --metadata Baking/Metadata/Metadata.fometa-server \
  --metadata Baking/Metadata/Metadata.fometa-client \
  --write
```

После baking в project CI используйте те же arguments с `--check`. Defaults outputs: `Docs/generated/project-remote-calls.json` и `Docs/generated/project-remote-calls.md`. Они содержат стабильные IDs `script.remote-call.<target>.<name>`, нормализованные signatures, surfaces caller/handler, per-call значения `MaxBytes` / `MaxCollectionSize`, input hashes и paired direction evidence. Каждая baked-запись должна содержать trailer `Limits`, включая явные нули для деклараций без структурных пределов. Намеренно публикуется только hint source file, потому что baked format не сохраняет repository-relative path и line.

Полный контракт declaration, runtime, authority, compatibility и troubleshooting приведён в разделе [Remote calls](../../reference/scripting/remote-calls.md). Принадлежащий движку minimal project проверяет decoder по реальному baker output, не создавая зависимости репозитория от внешней игры.

## Точки входа регистрации metadata

Hand-authored declarations находятся в `Source/Common/MetadataRegistration.h`:

- `RegisterServerMetadata()`
- `RegisterClientMetadata()`
- `RegisterMapperMetadata()`
- `RegisterServerStubMetadata()`
- `RegisterClientStubMetadata()`
- `RegisterMapperStubMetadata()`
- `RegisterDynamicMetadata()`
- `ReadMetadataBin()`
- `ReadMetadataVersion()`

`Source/Common/MetadataRegistration.template.cpp` является template side-specific registration files. Он содержит markers code generation, включая `///@ CodeGen RegisterHelpers` и `///@ CodeGen Register`.

`Source/Common/GenericCode.template.cpp` является template generated common code.

## Теги hooks движка

Код проекта или native extension может помечать выбранные C++ functions тегом `///@ EngineHook`. `BuildTools/codegen.py` проверяет имена hooks и создаёт no-op stubs для не реализованных встраиваемым проектом hooks. Сейчас generator распознаёт:

- `ApplicationInitHook(AppInitFlags, GlobalSettings&)`
- `ApplicationShutdownHook()`
- `ServerInitHook(ServerEngine*)`
- `ClientInitHook(ClientEngine*)`
- `ClientStartupSettingsHook(GlobalSettings&, int32_t clientIndex, bool embedded)`
- `SetupBakersHook(...)`
- `CheckCritterVisibilityHook(...)`
- `CheckItemVisibilityHook(...)`

`ClientStartupSettingsHook` вызывается entry points приложения непосредственно перед созданием client engine. Используйте его для project-owned корректировок startup settings, а не для обхода gameplay authority.

`ApplicationShutdownHook` является native lifecycle hook для project-owned process integrations, которые нужно остановить до выгрузки DLL client runtime. Он намеренно не входит в compatibility hash, потому что не меняет script metadata, saved data или network contract.

## Продвижение script `Entity`

У AngelScript type `Entity` нет единственного native counterpart. Code generator
продвигает его в entity class текущего target: `ServerEntity*` для server,
`ClientEntity*` для client и mapper и base `Entity*` для остальных targets.
Methods регистрируются на конкретных script entity types, где они доступны,
поэтому abstract receiver не позволяет обойти target ownership.

Script type system при этом разрешает casts `Proto*` и `Abstract*` к `Entity`.
Prototype и `ServerEntity` являются sibling native types, поэтому promotion
остаётся заявлением, которое нужно проверить на native boundary.
`NativeDataCaller` читает scalar handles, array elements и dictionary values
как base entities и сужает их к сгенерированному target type. Mismatch бросает
`ScriptException` с фактическими type и entity вместо передачи неверного sibling
pointer в callee. Arrays сохраняют прежнюю терпимость к destroyed entities.
Entity handles не поддерживаются как dictionary keys, а arrays entity handles,
вложенные в dictionary values, отклоняются во время compile, пока для этих slot
shapes не появится явный conversion contract.

## Динамические metadata

`Source/Common/MetadataRegistration.cpp` реализует `RegisterDynamicMetadata()`. Функция читает binary sections metadata и направляет их в typed steps регистрации:

- enums
- entities
- entity holders
- fixed/value/reference types
- properties
- events
- remote calls
- settings
- migration rules

Это runtime side metadata, которую можно загрузить из generated/baked data вместо одной только compiled static registration.

### Версия metadata

**Server и каждый подключённый client обязаны использовать metadata из одной bake.** Entity payload адресует properties по registration order этих metadata, поэтому в разных bakes один index может означать разные properties. Такое расхождение отклоняется как дефект build или deployment и не считается поддерживаемым compatibility mode.

Один `FO_COMPATIBILITY_VERSION` не может обеспечить этот invariant. Codegen видит metadata sources движка и C++-код встраивающего проекта, а project declarations `///@ Property` регистрируются runtime из baked script metadata. Поэтому binary compatibility version описывает executables, а layout properties принадлежит ресурсам.

`MetadataBaker` детерминированно выводит metadata version из всех разобранных codegen tags до target filtering. Client, server и mapper outputs одной bake получают общую версию, хотя их итоговые sections различаются. Любое изменение tag-level contract — порядок properties, layout fixed type, enum values, events, settings или remote calls — меняет эту версию.

Каждый файл `Metadata.fometa-*` начинается с fixed header перед section table:

| Поле | Тип | Назначение |
|------|-----|------------|
| magic | `uint32` | `METADATA_FILE_MAGIC`; сразу отклоняет foreign или truncated input |
| file version | `uint16` | `METADATA_FILE_VERSION`; mismatch требует rebake |
| metadata version | `uint16` length + bytes | детерминированная версия parsed tag stream |

Изменение token layout любой metadata section меняет file layout и требует bump
`METADATA_FILE_VERSION`. Metadata version не заменяет эту защиту: она хеширует
code-generation tags, поэтому изменение tokens reader/writer может не изменить
hash. Текущая file version равна `2`; более старый layout отклоняется с verdict
о необходимости rebake до начала decoding sections.

`MakeMetadataHeader()` и `ReadMetadataHeader()` владеют форматом в `MetadataRegistration.cpp`. `RegisterDynamicMetadata()` читает header до любой section и передаёт значение в `EngineMetadata::RegisterMetadataVersion()`. `ReadMetadataVersion()` читает только header для проверок updater и startup server; runtime получает зарегистрированное значение через `EngineMetadata::GetMetadataVersion()`. Значение вычисляется, а не настраивается. `Network.ForceMetadataVersion` существует только для имитации mismatch в tests.

Invariant обеспечивают четыре слоя:

1. Одна bake создаёт `Baking.ServerResources` и `Baking.ClientResources`, а deployment обновляет их вместе.
2. `UpdaterBackend::LoadFromClientResources` читает version из распространяемых client packs и завершает startup с `UpdaterException`, если она не совпадает с загруженной server version.
3. После sync `Updater::FinishResourcesUpdate` повторно читает local packs и возвращает `UpdaterResult::MetadataMismatch` до создания `ClientEngine`, если они всё ещё отличаются от server.
4. Client handshake отправляет свою version, а server возвращает собственную version и mismatch verdict как последнюю race-проверку. См. [Client updater](../../explanation/runtime/client-updater.md#handshake).

Deserialization имеет независимую защиту: `Properties::VerifyRestoredPropertyData()` проверяет, что каждая serialized property включена для target, не является virtual и имеет ожидаемый plain-data size. Она выбрасывает `VerificationException`, не доходя до strong assertion в `SetRawData`, поэтому foreign layouts остаются диагностируемыми.

При mismatch не отключайте проверку. Startup log server содержит `Metadata version:`, rejection paths называют обе версии, а updater log также указывает прочитанный resource directory. Найдите server или client resource directory из другой bake и повторно разверните согласованный комплект.

Focused coverage находится в `Test_MetadataBaker.cpp` (одна version для всех
targets, tag changes и отказ от старого file layout), `Test_Properties.cpp`
(`PropertiesRestoreRejectsForeignMetadata`) и
`Test_ClientServerIntegration.cpp`
(`ServerReportsMetadataMismatchInHandshake`).

Migration rules являются generic remaps `(kind, extra-info, target → replacement)` с transitive resolution и задаются как `///@ MigrationRule <Kind> ...`. Помимо `Proto` / `Property`, применяемых при lookup proto и resolution property name, kind `Enum` используется `PropertiesSerializer`, когда сохранённое **имя** enum value больше не разрешается при load. Вместо `EnumResolveException` rule сопоставляет старое имя текущему значению как для scalar enum properties, так и для enum keys словаря. Удалённые или переименованные enum values не делают старые saves непригодными.

## Properties и generated contracts

`Source/Common/Properties.h` и `Source/Common/Properties.cpp` определяют runtime model properties, используемую entities и metadata. Основные concepts:

- `PropertyRawData`
- `Property`
- `PropertyRegistrar`
- `Properties`
- callbacks getter/setter/post-set properties
- base type, struct layout и descriptors сериализации

Layouts fixed value types разделяются native C++, регистрацией AngelScript и traversal fields metadata. Поэтому `hstring` имеет explicit ABI invariant: `sizeof(hstring) == sizeof(hstring::hash_t) == 8` на каждом поддерживаемом target. На 32-bit targets pointer-backed handle содержит trailing padding, чтобы сохранить ширину и platform-independent offsets composite types, например `TextPackKey`. Padding не является wire data: serializers RPC/properties по-прежнему преобразуют handle через `as_hash()` и разрешают полученный hash через hash resolver target engine.

При изменении property metadata проверяйте одновременно runtime properties и inputs/templates generator. Изменения видимых скриптам nullability или API должны также обновлять [Scripting](../../explanation/scripting-runtime/), [карту script methods](../../reference/script-api/method-ownership.md) и [Nullability](../../contributing/coding-contracts/nullability.md), где это применимо.

## Связь с публичным API

[Индекс публичных контрактов](../../reference/public-contract/index.md) является
канонической human map всех восемнадцати modeled contract domains.
`BuildTools/docs_public_api.py` читает проверенные generated models и создаёт
согласованные English и Russian indexes; root [PUBLIC_API.md](../public-contract/index.md)
теперь является только durable legacy route. Индекс сообщает текущую stability
каждого domain и ведёт к human reference и machine model, не повышая доступные
implementation details до supported API.

Native model остаётся отдельным authoritative source текущей codegen surface:
для human browsing используйте [справочник native script API](../../reference/script-api/index.md),
для точного machine consumption — [generated/api.json](../../../generated/api.json).
Большинство найденных symbols наследуют default label `internal`, пока owners
не добавят explicit source-backed classifications; полный inventory не является
compatibility promise.

После изменения любой участвующей модели регенерируйте и проверяйте contract index:

```bash
python BuildTools/tests/test_docs_public_api.py
python BuildTools/docs_public_api.py --write
python BuildTools/docs_public_api.py --check
```

## Связь metadata и baker

Генерация metadata и их baking связаны, но не идентичны:

- Codegen создаёт generated C++/include files для compiled targets.
- `MetadataBaker` участвует в resource baking и создаёт side-specific metadata для runtime loading и проектной документации remote calls.
- `RegisterDynamicMetadata()` потребляет binary data metadata из ресурсов.

Подробности resource baking приведены в разделе [Pipeline выпечки](../../explanation/content-pipeline/baking.md).

## Какие тесты проверять

Связанные tests:

- `Source/Tests/Test_EngineMetadata.cpp`
- `Source/Tests/Test_MetadataBaker.cpp`
- `BuildTools/tests/test_docs_api.py`
- `BuildTools/tests/test_docs_api_diff.py`
- `BuildTools/tests/test_docs_contract_diff.py`
- `BuildTools/tests/test_docs_native_extension.py`
- `BuildTools/tests/test_docs_audio.py`
- `BuildTools/tests/test_docs_video.py`
- `BuildTools/tests/test_docs_gui_runtime.py`
- `BuildTools/tests/test_docs_ai_control_protocol.py`
- `BuildTools/tests/test_docs_public_api.py`
- `BuildTools/tests/test_docs_metadata.py`
- `Source/Tests/Test_Properties.cpp`
- Смежные с baker/codegen tests, включая `Test_BakerSetup.cpp`, и конкретные baker tests, когда metadata влияет на baked resources.

Если затронут generated script API, также проверяйте tests AngelScript.

## Маршрутизация изменений

- Arguments/output list CMake generator: `BuildTools/cmake/stages/Codegen.cmake`.
- Behavior generator script: `BuildTools/codegen.py`.
- Template static registration metadata: `Source/Common/MetadataRegistration.template.cpp`.
- Template generated common code: `Source/Common/GenericCode.template.cpp`.
- Runtime reader/registrar dynamic metadata: `Source/Common/MetadataRegistration.cpp`.
- Property model: `Source/Common/Properties.*` и код metadata entities/prototypes.
- Baking metadata resources: `Source/Tools/MetadataBaker.*` и [pipeline выпечки](../../explanation/content-pipeline/baking.md).
- Каталоги project remote calls: `BuildTools/docs_metadata.py` и [Remote calls](../../reference/scripting/remote-calls.md).
- Project-native roles/hooks/bindings: `BuildTools/NativeExtensionInterface.json`, `BuildTools/docs_native_extension.py` и [Native extensions](../../how-to/native-extensions.md).
- Формат прототипов и built-in authoring metadata: `BuildTools/PrototypeFormatInterface.json`, `BuildTools/docs_prototype_format.py` и [формат прототипов](../../how-to/content/prototype-format.md).
- Формат карт, ownership placements, normalization mapper и bake/materialization карт: `BuildTools/MapFormatInterface.json`, `BuildTools/docs_map_format.py` и [формат карт](../../how-to/content/map-format.md).
- Font descriptors, slot binding, text layout и rendering: `BuildTools/FontFormatInterface.json`, `BuildTools/docs_font_format.py` и [форматы шрифтов](../../how-to/content/font-format.md).
- Delivery, decoding, playback и mixing аудио: `BuildTools/AudioInterface.json`, `BuildTools/docs_audio.py` и [аудиоресурсы](../../how-to/content/audio.md).
- Delivery и playback Ogg/Theora: `BuildTools/VideoInterface.json`, `BuildTools/docs_video.py` и [видеоресурсы](../../how-to/content/video.md).
- Переиспользуемые types AngelScript GUI, lifecycle, layout и input: `BuildTools/GuiRuntimeInterface.json`, `BuildTools/docs_gui_runtime.py` и [GUI runtime](../../how-to/runtime/gui.md).
- Project-neutral envelope AI control и reference client: `BuildTools/AiControlProtocol.json`, `BuildTools/docs_ai_control_protocol.py` и [протокол AiControl](../../how-to/ai-control-protocol.md).
- Aggregate human routing контрактов: `BuildTools/docs_public_api.py` и [индекс публичных контрактов](../../reference/public-contract/index.md).
- Discovery evidence внешних проектов и visual assets документации: `BuildTools/docs_external_evidence.py`, `BuildTools/docs_diagrams.py` и `BuildTools/docs_screenshots.py`.
- Сравнение revisions generated contracts и dispositions: `BuildTools/docs_contract_diff.py`, native layer `BuildTools/docs_api_diff.py` и [управление изменениями контрактов](../../contributing/contract-change-management.md).
- Script runtime и script-visible signatures: [Scripting](../../explanation/scripting-runtime/), [карта script methods](../../reference/script-api/method-ownership.md) и [Nullability](../../contributing/coding-contracts/nullability.md).

## Контрольный список проверки

1. Выполните configure из корня встраиваемого проекта, чтобы были доступны project metadata sources.
2. Запустите обычную code generation и убедитесь, что generated files обновились ожидаемо.
3. При изменении caching/dependency behavior generator запустите forced code generation.
4. Соберите минимальный target, компилирующий generated files.
5. Запустите относящиеся к изменению tests metadata/properties.
6. Если metadata запекаются, запустите соответствующий baker test и bake target.
7. Для project remote calls запустите `BuildTools/tests/test_docs_metadata.py`, затем регенерируйте и проверьте paired catalog из текущей выпечки.
8. Для native extensions выполните focused checks Python/CMake и путь minimal starter либо затронутый project runtime.
9. Для prototype format выполните focused test/check generator и повторную выпечку затронутого проекта; при изменении custom metadata или комбинаций fields добавьте project semantic validation.
10. Для map format выполните focused test/check generator, затронутые unit tests карт и bake встраиваемого проекта; при изменении custom metadata или semantics карт добавьте project content validation.
11. Сравните все generated contracts командой `BuildTools/docs_contract_diff.py --write --enforce`; для глубокой диагностики native symbols используйте специализированный API comparator и заполните точные dispositions для каждого required break.
12. Обновите docs, раскрывающие изменённые public contracts, прежде всего [Scripting](../../explanation/scripting-runtime/), [Remote calls](../../reference/scripting/remote-calls.md), [Native extensions](../../how-to/native-extensions.md), [формат прототипов](../../how-to/content/prototype-format.md), [формат карт](../../how-to/content/map-format.md), [карту script methods](../../reference/script-api/method-ownership.md), [Nullability](../../contributing/coding-contracts/nullability.md) и [индекс публичных контрактов](../../reference/public-contract/index.md).
13. Запустите tests API/reference и команды `python BuildTools/docs_api.py --check`, `python BuildTools/docs_reference.py --check`; при изменении metadata регенерируйте оба layers.
14. Запустите focused test и `--check` каждого затронутого domain generator; для cross-domain change выполните `docs_contract_diff.py --enforce` и регенерируйте public contract index.
15. После изменения public Markdown, paths, diagrams или screenshots по порядку регенерируйте snippets, localization status, site data, AI evaluation/delivery, затем проверьте built Jekyll artifact и browser audit.
