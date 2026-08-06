---
layout: default
title: Управление изменениями генерируемых контрактов
locale: ru
document_id: api-change-management
permalink: /Docs/ru/contributing/contract-change-management.html
---

<!-- docs-translation: {"document_id":"api-change-management","locale":"ru","source_path":"Docs/en/contributing/contract-change-management.md","source_sha256":"11c39819bab854b6cb7a2bcecb9799d80b7397ebfa03a5eac00b515db488c480"} -->

# Управление изменениями генерируемых контрактов

> Руководство для сопровождающих движок. Используйте эту страницу, чтобы сравнивать между ревизиями генерируемые контракты native API, CMake, основного и вспомогательного BuildTools CLI, package, native extensions, форматов prototype, map, model, text, effect, image, particle и font, audio, video, GUI runtime и протокола AiControl и принимать решения по изменениям, чувствительным к совместимости, до слияния.

## Назначение

FOnline публикует восемнадцать детерминированных машиночитаемых моделей в `Docs/generated/`. `BuildTools/docs_contract_diff.py` сравнивает все восемнадцать с теми же моделями на базовой ревизии и создаёт один JSON-отчёт для автоматизации и один Markdown-отчёт для review.

Gate отвечает на четыре отдельных вопроса:

1. Какой домен и какая стабильная запись изменились?
2. Является ли изменение additive, только документационным, только policy или структурно breaking?
3. Обещала ли baseline-ревизия совместимость для этой записи или домена?
4. Если требуется review, где записаны решение владельца, миграция, release note и обработка совместимости?

Comparator сообщает о внутреннем churn, но не повышает internal surface до публичного API. Стабильность остаётся принадлежащей исходникам: native symbols используют `///@ ApiContract`; модели CMake, main CLI, package, helper CLI, native extension, prototype format, map format, model format, text format, effect format, image format, particle format, font format, audio, video, GUI runtime и AiControl protocol используют объявленную стабильность домена или записи.

Classification и stability являются разными стадиями. Сначала классифицируйте
наблюдаемое изменение shape/prose/policy как additive, documentation, policy или
breaking; затем используйте baseline stability, чтобы решить, требуется ли
проверенный disposition. Ledger записывает disposition maintainer и не выполняет
автоматическую классификацию. Ни `--write`, ни `--enforce` не создают решения о
миграции, release note, compatibility или owner.

## Решение о disposition

Сравнивайте нормализованные машинные модели по stable IDs и сохраняйте
`contract_sha256` каждой модели вместе с source provenance, чтобы шум
форматирования или порядка не выглядел как изменение совместимости. Для каждой
записи, требующей ревью, назначьте стабильный `change_id` и до merge заполните
поля disposition ledger `migration`, `release_note`, `compatibility` и `owner`.
Сгенерированный diff находит изменения, но не принимает за владельца решение о
миграции или релизе.

## Охватываемые домены

| Домен | Каноническая модель | Сопоставление записей | Текущая проверка |
| --- | --- | --- | --- |
| Native API | [generated/api.json](../../generated/api.json) | `id` нативного символа; перегрузки сохраняют IDs, хешированные по сигнатуре | Breaking changes baseline `stable`, `experimental` и `deprecated` требуют disposition; internal changes остаются видимыми |
| CMake | [generated/cmake.json](../../generated/cmake.json) | IDs `cmake.option.*`, `cmake.stage.*` и `cmake.helper.*` | Домен `experimental`, поэтому removals и shape changes требуют disposition |
| BuildTools CLI | [generated/cli.json](../../generated/cli.json) | IDs команд и аргументов `cli.*` | Домен `internal`; breaking changes сообщаются, но не создают обещание совместимости |
| Package | [generated/package.json](../../generated/package.json) | IDs declaration, option, target, platform, pack, payload и argument `package.*` | Домен `internal`; breaking changes сообщаются, но не создают обещание совместимости |
| Helper CLI | [generated/helper-cli.json](../../generated/helper-cli.json) | IDs helper, subcommand и argument `helper-cli.*` | Домен `internal`; изменения parser или ownership сообщаются, но не создают обещание совместимости |
| Native extension | [generated/native-extension.json](../../generated/native-extension.json) | IDs role, hook и binding rule `native-extension.*` | Домен `experimental`; removals и structural changes требуют disposition, а бинарная совместимость между независимо собранными ревизиями не обещается |
| Prototype format | [generated/prototype-format.json](../../generated/prototype-format.json) | IDs section, directive, rule, built-in entity и property `prototype-format.*` | Grammar и rules имеют статус `experimental` и при поломке требуют disposition; производный property catalog остаётся `internal` и видимым без дополнительного обещания совместимости |
| Map format | [generated/map-format.json](../../generated/map-format.json) | IDs section, directive, ownership, rule и property `map-format.*` | Grammar, ownership и rules имеют статус `experimental` и при поломке требуют disposition; производный от ревизии property catalog остаётся `internal` |
| Model format | [generated/model-format.json](../../generated/model-format.json) | IDs compile limit, asset, token и rule `model-format.*` | Grammar и composition rules имеют статус `experimental` и при поломке требуют disposition; compile limits и производные факты о ресурсах видимы с объявленной стабильностью |
| Text format | [generated/text-format.json](../../generated/text-format.json) | IDs syntax, language, prototype text, runtime, rendering и validation `text-format.*` | Контракт parser, baker и runtime имеет статус `experimental`; removals и structural changes требуют disposition, а языки проекта и formatter policy остаются вне модели |
| Effect format | [generated/effect-format.json](../../generated/effect-format.json) | IDs compile limit, section, option, resource, baking, runtime, script method и validation `effect-format.*` | Контракт baker, renderer и runtime имеет статус `experimental`; removals и structural changes требуют disposition, а shader catalogs проекта, visual policy и значения ScriptValue остаются вне модели |
| Image format | [generated/image-format.json](../../generated/image-format.json) | IDs source format, FOFRM field, filename option, baking, runtime и validation `image-format.*` | Контракт baker и default client имеет статус `experimental`; private container entries имеют статус `internal`, а project asset catalogs, licenses, pack precedence, visual policy и acceptance остаются вне модели |
| Particle format | [generated/particle-format.json](../../generated/particle-format.json) | IDs backend/format, registered object/family, XML, renderer, tooling, runtime, integration и validation `particle-format.*` | Контракт `.spark`/`.spk`, `.efkproj`/`.efk` и интеграции Engine имеет статус `experimental`; производные записи native coverage остаются `internal`, а project catalogs, settings, effects, textures, models, budgets и visual acceptance остаются вне модели |
| Font format | [generated/font-format.json](../../generated/font-format.json) | IDs descriptor format/field, binding, layout, rendering и validation `font-format.*` | Контракт FOFNT/BMFont и client text pipeline имеет статус `experimental`; cache internals остаются `internal`, а project slot assignment, glyph coverage, typography и visual acceptance остаются вне модели |
| Audio | [generated/audio.json](../../generated/audio.json) | IDs format, delivery, decoding, playback и validation `audio.*` | Доставка WAV/ACM/Ogg и client playback имеют статус `experimental`; записи documentation/test gap остаются `internal`, а project catalogs, spatial/music policy, mastering, licensing и audible acceptance остаются вне модели |
| Video | [generated/video.json](../../generated/video.json) | IDs format, delivery, decoding, fullscreen, embedded и validation `video.*` | Ogg/Theora и client presentation имеют статус `experimental`; missing fixture и loop risk остаются явными, а project cinematics, subtitles, policy, assets, provenance, budgets и visible acceptance остаются вне модели |
| GUI runtime | [generated/gui-runtime.json](../../generated/gui-runtime.json) | IDs type, screen API, annotation, lifecycle, layout, input, integration и validation `gui-runtime.*` | Повторно используемый контракт CoreScripts GUI имеет статус `experimental`; declarative formats, generators, screens, styles, assets, accessibility policy и visible acceptance остаются проектными |
| AiControl protocol | [generated/ai-control-protocol.json](../../generated/ai-control-protocol.json) | IDs transport, method, command/event, security, integration и validation `ai-control-protocol.*` | Повторно используемые wire и control protocol имеют статус `experimental`; game-specific schemas, actions, administrator tools и MCP namespaces остаются проектными |

Изменения источника модели, repository/scope или контракта уровня модели считаются консервативными domain breaks и всегда требуют disposition. Так comparator или граница владения не могут молча переопределить охват gate.

Авторские remote calls проекта остаются отдельным запечённым проектным каталогом. Для включения остальных авторских форматов файлов, поведения updater и ключей конфигурации проекта в comparator сначала требуется владеющая модель.

## Проверенные исходные пути

- `BuildTools/docs_api.py`
- `BuildTools/docs_api_diff.py`
- `BuildTools/docs_cmake.py`
- `BuildTools/docs_cli.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/docs_helper_cli.py`
- `BuildTools/NativeExtensionInterface.json`
- `BuildTools/docs_native_extension.py`
- `BuildTools/PrototypeFormatInterface.json`
- `BuildTools/docs_prototype_format.py`
- `BuildTools/MapFormatInterface.json`
- `BuildTools/docs_map_format.py`
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
- `BuildTools/VideoInterface.json`
- `BuildTools/docs_video.py`
- `BuildTools/GuiRuntimeInterface.json`
- `BuildTools/docs_gui_runtime.py`
- `BuildTools/AiControlProtocol.json`
- `BuildTools/docs_ai_control_protocol.py`
- `BuildTools/docs_package.py`
- `BuildTools/docs_contract_diff.py`
- `BuildTools/docs_validate.py`
- `BuildTools/tests/test_docs_api_diff.py`
- `BuildTools/tests/test_docs_contract_diff.py`
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
- `BuildTools/tests/test_docs_ai_control_protocol.py`
- `Docs/generated/api.json`
- `Docs/generated/cmake.json`
- `Docs/generated/cli.json`
- `Docs/generated/helper-cli.json`
- `Docs/generated/native-extension.json`
- `Docs/generated/prototype-format.json`
- `Docs/generated/map-format.json`
- `Docs/generated/model-format.json`
- `Docs/generated/text-format.json`
- `Docs/generated/effect-format.json`
- `Docs/generated/image-format.json`
- `Docs/generated/particle-format.json`
- `Docs/generated/font-format.json`
- `Docs/generated/audio.json`
- `Docs/generated/video.json`
- `Docs/generated/gui-runtime.json`
- `Docs/generated/ai-control-protocol.json`
- `Docs/generated/package.json`
- `Docs/contract-change-dispositions.json`
- `Docs/en/contributing/decisions/0002-public-api-stability-contract.md`
- `.github/workflows/validate.yml`

## Входы и результаты

Aggregate diff использует:

- один baseline directory или Git revision, содержащие все доступные канонические модели;
- текущий каталог моделей `Docs/generated/`;
- накопительный реестр `Docs/contract-change-dispositions.json`.

Он записывает artifacts пары ревизий под игнорируемым `Workspace/`:

- `contract-diff.json` для автоматизации и AI review;
- `contract-diff.md` для сопровождающих.

GitHub Actions загружает их как `contract-diff-<commit-sha>` на 14 дней. Это диагностические artifacts, а не текущие reference pages, поэтому сайт GitHub Pages продолжает отображать только текущий Markdown и сгенерированные справочники.

## Стабильное сопоставление и нормализация

Каждый домен сравнивает стабильные IDs, а не display names или позиции в JSON arrays. Добавления и удаления никогда не образуют пару по похожей подписи.

Native comparator сохраняет специфичное для символов поведение:

- изменение сигнатуры без перегрузок обычно изменяет один символ;
- изменение сигнатуры перегрузки выглядит как один удалённый ID и один добавленный ID под одним `family_id`;
- перемещение source path и line не может стать ложным breaking change.

Comparators CMake, main CLI, package, helper CLI, native extension, prototype format, map format, model format, text format, effect format, image format, particle format, font format, audio, video, GUI runtime и AiControl protocol разворачивают принадлежащие модели коллекции записей по стабильному ID. Source provenance, включая производные enum source paths/line numbers, generated summaries, derived usage strings и model digests, не создают повторных изменений. Вложенные изменения description/help остаются documentation changes; defaults, choices, cardinality, required flags, ownership/invocation metadata, platform/target matrices, signatures, stage order, hook fallbacks/call sites, role routing, применимость grammar и resources prototype/map/model/text/effect/image/particle/font/audio/video, compile limits, language normalization, runtime lookup, поведение shader/image/particle/font/audio/video/GUI/AiControl runtime и payload semantics являются структурными данными контракта.

Каждый домен записывает два хеша:

- `model_sha256` охватывает точный вход канонической модели;
- `contract_sha256` исключает provenance исходников и описательную прозу, сохраняя shape и policy.

Disposition привязан к baseline и current contract hash затронутого домена. Несвязанный текст или перемещение строк исходника не могут сделать его недействительным, а следующее изменение контракта может.

## Автоматические классификации

| Классификация | Примеры | Влияние на merge |
| --- | --- | --- |
| `additive` | Новый symbol, option, command, argument, platform, pack или payload entry | Отображается; breaking disposition не требуется |
| `documentation` | Только description, example, summary, notes или help prose | Отображается; breaking disposition не требуется |
| `policy` | Metadata stability/since/support без понижения | Отображается; breaking disposition не требуется |
| `breaking` | Removal; изменение signature/type/default/required/choice/order/matrix/payload shape; stability withdrawal | Disposition зависит от baseline stability, кроме изменений source/scope модели, которые требуют его всегда |

Проверкой управляет старая ревизия. Изменение не может переименовать baseline-public surface в `internal` и одновременно удалить или изменить её shape, чтобы обойти review.

| Стабильность baseline | Breaking change записи |
| --- | --- |
| `stable` | Блокируется до записи обработки migration, release, compatibility и owner |
| `experimental` | Блокируется до того, как владелец запишет изменение и release/migration disposition |
| `deprecated` | Блокируется до явного решения о замене и сроке удаления |
| `internal` | Видно в отчёте; compatibility disposition не требуется |

## Локальный процесс

Сначала пересоберите и проверьте каждую текущую модель:

```bash
python BuildTools/docs_api.py --write
python BuildTools/docs_reference.py --write
python BuildTools/docs_cmake.py --write
python BuildTools/docs_cli.py --write
python BuildTools/docs_helper_cli.py --write
python BuildTools/docs_native_extension.py --write
python BuildTools/docs_prototype_format.py --write
python BuildTools/docs_map_format.py --write
python BuildTools/docs_model_format.py --write
python BuildTools/docs_text_format.py --write
python BuildTools/docs_effect_format.py --write
python BuildTools/docs_image_format.py --write
python BuildTools/docs_particle_format.py --write
python BuildTools/docs_font_format.py --write
python BuildTools/docs_audio.py --write
python BuildTools/docs_video.py --write
python BuildTools/docs_gui_runtime.py --write
python BuildTools/docs_ai_control_protocol.py --write
python BuildTools/docs_package.py --write
```

Сравните с целевой базой интеграции:

```bash
python BuildTools/docs_contract_diff.py \
  --baseline-git-ref origin/master \
  --current-dir Docs/generated \
  --dispositions Docs/contract-change-dispositions.json \
  --json-output Workspace/contract-diff.json \
  --markdown-output Workspace/contract-diff.md \
  --write \
  --enforce
```

Для явно сохранённого локального baseline со всеми восемнадцатью файлами моделей:

```bash
python BuildTools/docs_contract_diff.py \
  --baseline-dir Workspace/contract-baseline \
  --current-dir Docs/generated \
  --check \
  --enforce
```

`--check` вычисляет и проверяет результат, не записывая файлы отчёта. При устранении ошибки предпочтителен `--write`, поскольку Markdown-отчёт включает готовые для заполнения шаблоны реестра.

`BuildTools/docs_api_diff.py` остаётся доступным для исследования нативных символов и regression tests. CI использует aggregate command, поэтому зелёный отчёт только API не может скрыть drift CMake, CLI, package, helper CLI, native extension, prototype format, map format, model format, text format, effect format, image format, particle format, font format, audio, video, GUI runtime или AiControl protocol.

## Поведение bootstrap

`--allow-missing-baseline` предназначен только для первой ревизии, которая добавляет каноническую модель в существующую ветку. При Git baseline только отсутствующие домены получают видимый статус `bootstrap`; существующие домены всё равно сравниваются и проверяются. Неизвестная или незагруженная Git revision остаётся ошибкой.

Directory baseline обязан содержать все восемнадцать моделей. Это предотвращает случайные частичные локальные сравнения, выглядящие полными.

После добавления всех моделей отсутствие baseline files не является нормой. Не добавляйте bootstrap mode в локальные команды или CI только для обхода упавшего сравнения.

## Реестр решений

`Docs/contract-change-dispositions.json` является накопительным реестром schema v2. Каждая запись содержит:

- `domain`: `api`, `cmake`, `cli`, `package`, `helper-cli`, `native-extension`, `prototype-format`, `map-format`, `model-format`, `text-format`, `effect-format`, `image-format`, `particle-format`, `font-format`, `audio`, `video`, `gui-runtime` или `ai-control-protocol`;
- детерминированный `change_id` с префиксом домена;
- значения `contract_sha256` baseline и current домена;
- классификацию владельца: `breaking` или `compatible`;
- непустые поля rationale, migration, release-note, compatibility и owner.

Пример:

```json
{
  "domain": "cmake",
  "change_id": "cmake-change.modified.0123456789abcdef",
  "baseline_contract_sha256": "<64 lowercase hex characters>",
  "current_contract_sha256": "<64 lowercase hex characters>",
  "classification": "breaking",
  "rationale": "Why the change is intentional and what embedding projects observe.",
  "migration": "Docs/Migrations/Next.md#changed-option",
  "release_note": "Docs/ReleaseNotes/Next.md#changed-option",
  "compatibility": "Pinned projects must update the option and engine revision together.",
  "owner": "build-release"
}
```

Исторические записи могут оставаться; записи без совпадения инертны. Домен, change ID и оба hash обязаны совпадать. Классификация обнаруженного изменения как `compatible` не отменяет остальные поля. Если migration или release note не нужны, запишите проверенную причину вместо пустого значения.

Validator доказывает форму реестра и точную привязку, но не качество одобрения. Review обязан отклонять placeholder text, неверные заявления о совместимости, отсутствующие migrations или владельца вне затронутого домена.

## Поведение CI

Задание `Validate documentation` загружает полную историю и выбирает:

- `github.event.pull_request.base.sha` для pull requests;
- `github.event.before` для pushes в `master`.

После проверки актуальности моделей и справочников CI выполняет `docs_contract_diff.py --write --enforce`. Отсутствующее обязательное disposition завершает задание ошибкой. Шаг upload с `if: always()` сохраняет оба отчёта для диагностики.

Pull request с несколькими commits сравнивается с commit базовой ветки, а не с предыдущим commit feature branch. Push с несколькими commits использует полный отправленный диапазон. Standalone validator отклоняет удаление общего реестра, контракта манифеста с восемнадцатью моделями, checkout полной истории, аргумента base ref, aggregate test или enforcement switch.

## Что требует проверки человеком

Для публичного breaking change проверьте всё перечисленное:

1. Изменение source declaration или manifest намеренно.
2. Существует путь замены или миграции либо реестр объясняет, почему он невозможен.
3. Release notes называют затронутых разработчиков и поддерживаемые линии ревизий.
4. Изменения network/serialization включают обязательные metadata совместимости или миграции.
5. Generated references, examples и focused tests изменяются вместе.
6. Владелец домена runtime, scripting, build или release принимает срок поддержки.

Внутренние изменения не требуют обещания совместимости, но неожиданные removals и shape churn всё равно заслуживают review, поскольку могут показать отсутствующую классификацию стабильности.

## Ограничения статического сравнения

Aggregate report не может обнаружить:

- изменения поведения за неизменными объявлениями;
- updater protocol, authored formats или project settings без канонических моделей;
- неверный текст, заявляющий совместимость;
- был ли успешно выполнен migration guide или package path;
- поддержку между линиями выпусков, которые ещё не объявлены и не отмечены tags.

По-прежнему обязательны тесты runtime, structural CMake, native extension, prototype/map/model/text/effect/image/particle/font/audio/video/GUI/AiControl protocol, package, starter и embedding project. Зелёный отчёт восемнадцати доменов доказывает только принятую смену ревизии для моделируемых declarative surfaces.

## Устранение неполадок

- `Contract baseline git revision is unavailable`: загрузите точную ревизию; не используйте bootstrap, чтобы скрыть shallow checkout.
- `does not exist at baseline revision`: ожидается только для первой зафиксированной ревизии модели.
- `missing dispositions`: проверьте `Workspace/contract-diff.md`, добавьте проверенные записи и повторите запуск для той же пары ревизий.
- disposition всё ещё отсутствует: скопируйте из текущего отчёта точные domain, change ID и оба contract hash.
- изменение description классифицировано как breaking: добавьте focused regression вложенного поля до изменения policy или ledger.
- internal change неожиданно блокирует: сначала проверьте drift source/scope модели; обычные изменения внутренних записей только отображаются в отчёте.
- одновременно изменилось много записей: проверьте stable IDs и владеющий generator до принятия отчёта.

## Контрольный список проверки

1. Пересоберите и проверьте все восемнадцать канонических моделей и сгенерированный Markdown.
2. Выполните `test_docs_api_diff.py` и `test_docs_contract_diff.py`.
3. Сравните с целевой базой при помощи `--write --enforce`.
4. Проверьте каждое обязательное disposition и замените каждый сгенерированный placeholder.
5. Выполните затронутый native, script, CMake, packaging, starter или project test.
6. Выполните `test_docs_validate.py` и `docs_validate.py`.
7. Проверьте CI artifact contract diff и статус каждого домена.
8. Не добавляйте файлы в staging, если staging или commit явно не запрошены.

## См. также

- [GeneratedApiAndMetadata.md](../reference/metadata/index.md): владение каноническими моделями и сгенерированными справочниками.
- [ADR-0002](decisions/0002-public-api-stability-contract.md): принятая политика стабильности и изменений.
- [Remote Calls](../reference/scripting/remote-calls.md): проектный каталог remote calls и граница совместимости.
- [Сопровождение документации](documentation/): процесс изменения документации и сверки обновлений ревизий.
- [Индекс публичных контрактов](../reference/public-contract/index.md): сгенерированный междоменный индекс публичных контрактов.
