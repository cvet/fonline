---
layout: default
title: Поддержание здоровья игрового проекта: форматирование, анализ и проверки
locale: ru
document_id: project-quality-gates
permalink: /Docs/ru/how-to/project/quality-gates.html
---

# Поддержание здоровья игрового проекта: форматирование, анализ и проверки
<!-- docs-translation: {"document_id":"project-quality-gates","locale":"ru","source_path":"Docs/en/how-to/project/quality-gates.md","source_sha256":"36f1fced7f48226efa5dd053e4d2492aa36e3c0d9fb2ec6d9d470fd0b676d197"} -->
Руководство разделяет то, что встраивающий игровой проект получает от движка без
какой-либо работы, и то, что проект должен добавить сам. Оно описывает
дисциплину, благодаря которой эти проверки остаются осмысленными: предупреждения
считаются ошибками, у исключений есть письменные причины, сгенерированные файлы
проверяются, а не правятся вручную, и проверка никогда не ослабляется ради
зелёного прогона. В конце даны единый порядок верификации и размещение каждой
проверки в pre-commit, локальной агрегирующей задаче и CI.

Движок владеет механизмами и собственным CI. Он не владеет раскладкой
форматтера игры, уровнями серьёзности анализаторов, baseline-файлами и порогами
приёмки, поэтому страница явно называет решения, которые принимает проект.

## Проверенные исходные пути

- `AGENTS.md`
- `BuildTools/buildtools.py`
- `BuildTools/package.py`
- `BuildTools/docs_support_matrix.py`
- `BuildTools/README.md`
- `BuildTools/format-source.sh`
- `BuildTools/format-source.cmd`
- `BuildTools/check_native_source_layout.py`
- `BuildTools/check_native_modules.py`
- `BuildTools/native_source_audit.py`
- `BuildTools/native_safety_audit.py`
- `BuildTools/check_script_bindings.py`
- `BuildTools/check_angelscript_hygiene.py`
- `BuildTools/format_content.py`
- `BuildTools/migration_guard.py`
- `BuildTools/check_text_packs.py`
- `BuildTools/markdown_links.py`
- `BuildTools/NativeModuleExceptions.json`
- `BuildTools/temporary_compat.py`
- `BuildTools/docs_metadata.py`
- `BuildTools/tests/test_docs_angelscript_style.py`
- `BuildTools/cmake/stages/Init.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/RunTestExecutable.cmake`
- `Source/.clang-format`
- `Source/Common/Settings.inc`
- `Source/Tools/ManagedScriptBaker.cpp`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/Managed/Analyzers/FOnline.Analyzers.csproj`
- `Docs/SyncCoverAnalysis.md`
- `Docs/en/how-to/scripting/style-and-refactoring.md`
- `Docs/en/how-to/scripting/managed-csharp.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/contributing/coding-contracts/nullability.md`
- `Docs/en/contributing/coding-contracts/thread-safety-analysis.md`
- `Docs/en/contributing/coding-contracts/exception-safety.md`
- `Docs/en/contributing/coding-contracts/local-variables.md`
- `Docs/en/contributing/coding-contracts/smart-pointers.md`
- `Docs/en/contributing/testing/index.md`
- `Docs/en/reference/buildtools/commands.md`
- `Examples/NativeExtensionSample/CMakeLists.txt`
- `Examples/MinimalMultiplayer/CMakeLists.txt`
- `Examples/MinimalMultiplayer/generate_config.py`
- `.github/workflows/validate.yml`

## Что поставляет движок и что добавляет проект

| Область | Поставляется движком | Добавляется проектом |
|---|---|---|
| Форматирование нативного кода | `buildtools.py format-source` для `Source/` движка; переопределение бинарного файла через `FO_CLANG_FORMAT`; проверка major-версии clang-format 20 | Собственный `.clang-format`, закреплённый бинарный файл и команда, обходящая каталоги проекта |
| Форматирование AngelScript | Исправление `T?`, приведений, `Item?[]` и именованных аргументов после clang-format (`fix_fos_nullable_suffix`); `buildtools.py format-content --fos <glob>` применяет его к скриптам проекта | Glob скриптов и `.clang-format` проекта |
| Форматирование контента | `buildtools.py format-content` для главной конфигурации и файлов прототипов с режимом проверки расхождений `--check` ([Форматирование файлов игрового контента](format-content.md)) | Его запуск; форматтеры только для проекта в обёртке |
| Проверки компиляции Managed C# | Сгенерированный проект с `Nullable=enable`, `CheckForOverflowUnderflow`, `TreatWarningsAsErrors`, `EnforceCodeStyleInBuild` | Уровни серьёзности в `.editorconfig`, ссылки на анализаторы и пакеты |
| Managed-анализаторы | `FOnline.Analyzers.csproj` с диагностиками покрытия сущностей `FOSYNC` и [правилами контрактов движка](engine-contract-analyzers.md) `FOENTRY`, `FOVERIFY` и `FOSTATIC` | Подключение через `ManagedScript.Analyzers`; уровни серьёзности; правила проекта в анализаторе проекта |
| Анализ потокобезопасности нативного кода | `-Werror=thread-safety` в каждой конфигурации Clang, применяется к коду движка и проекта | Аннотирование блокировок проекта; линия сборки на Clang |
| Предупреждения нативного кода | MSVC `/W4` без `/WX` | `-Werror` или `/WX` для целей, принадлежащих проекту |
| Компиляция скриптов | Цели `CompileAngelScript` и `CompileManagedScripts` | Запуск для каждой роли и сбой при предупреждениях |
| Аудиты нативного кода и его безопасности | `native_source_audit.py` (выделение памяти, умные указатели, локальные переменные) и `native_safety_audit.py` (покрытие сущностей, безопасность исключений), каждый с baseline движка ([аудит нативного кода](native-source-audits.md), [аудит безопасности нативного кода](native-safety-audits.md)) | Области, конфигурация и baseline-файлы проекта; compile database и инструменты Clang 20 для аудитов на основе Clang; политика CI |
| Проверки скриптовых привязок и гигиены AngelScript | `check_script_bindings.py` и `check_angelscript_hygiene.py` с baseline-храповиком по счётчикам ([проверки скриптов](script-and-binding-checks.md)) | Области, конфигурация, baseline-файлы; правила только для проекта в отдельном скрипте |
| Проверки контента и документации | `check_text_packs.py` ([текстовые пакеты](content-and-diagnostics-tools.md)), `migration_guard.py` ([защита миграций](migration-guard.md)), `markdown_links.py` ([проверка документации](documentation-checks.md)) | Языки и письменности, манифест идентичностей, набор документов и исключения с причинами |
| Проверки сгенерированного вывода | Соглашение `--check` в генераторах движка; `docs_metadata.py --check` | `--check` для каждого генератора проекта |
| Проверки раскладки, комментариев и дат совместимости | `check_native_source_layout.py` (с `--comments` и baseline комментариев) и `temporary_compat.py` принимают пути проекта | Решение их запускать |
| Санитайзеры | Типы сборки `San_*` в `Init.cmake` | Presets, которые их выбирают |

## Единообразное форматирование исходников

### Нативный C++ и C#

`python BuildTools/buildtools.py format-source` (обёртки: `BuildTools/format-source.sh`, `BuildTools/format-source.cmd`) форматирует только дерево `Source/` движка по шаблонам `**/*.cpp`, `**/*.h` и `**/*.cs`. Он не находит `SourceExt/` или `Scripts/` игры. Команда не читает аргументов (см. [сгенерированный справочник CLI](../../reference/buildtools/commands.md)).

Бинарный файл определяется в следующем порядке, и результат должен сообщать major-версию 20:

1. путь из `FO_CLANG_FORMAT`, если переменная задана;
2. `clang-format-20` в `PATH`;
3. `clang-format` в `PATH`.

Укажите в `FO_CLANG_FORMAT` единственный бинарный файл, который закрепляет проект, чтобы каждая рабочая станция и каждый runner CI форматировали одинаково.

```bash
export FO_CLANG_FORMAT="$PWD/Tools/clang-format-20"
```

```powershell
$env:FO_CLANG_FORMAT = "$PWD\Tools\clang-format-20.exe"
```

Движок поставляет `Source/.clang-format` с секциями `Cpp` и `CSharp`, и C# движка тоже форматируется clang-format. Файла стиля для игры движок не поставляет, поэтому проект владеет собственным `.clang-format` и не должен полагаться на родительский каталог за пределами своего репозитория.

### Синтаксис nullable в AngelScript

Необработанный clang-format разбирает `.fos` как C++. Он считает `?` бинарным оператором и переписывает nullable-маркеры и именованные аргументы AngelScript:

```text
Before: Critter? target;     Item?[] items;     cast<Critter?>(x)     Create(count: 2)
Raw:    Critter ? target;    Item ? [] items;   cast < Critter ? > (x)   Create(count : 2)
```

Никогда не запускайте необработанный clang-format над `.fos`-файлами. Исправление реализовано в `BuildTools/buildtools.py` как `fix_fos_nullable_suffix`. Оно маскирует строковые литералы, символьные литералы и комментарии, восстанавливает пробелы в приведённых выше формах и оставляет текст литералов без изменений. `format_files(clang_format, root, patterns)` в том же модуле оборачивает clang-format, удаляет UTF-8 BOM, сохраняет окончания строк LF или CRLF, оставляет один завершающий перевод строки, применяет исправление к каждому пути `.fos` и возвращает число изменённых файлов.

Сам `format-source` до `.fos`-файлов не добирается: в его списке шаблонов нет `*.fos`, а в дереве `Source/` движка таких файлов нет. Маршрут командной строки для скриптов проекта — `buildtools.py format-content --fos <glob>`: он применяет то же исправление к каждому файлу, сохраняет BOM, а не удаляет его, и имеет режим `--check` ([Форматирование файлов игрового контента](format-content.md)). Проект, которому нужна сама обёртка, вызывает функции напрямую. Собственный тест движка (`BuildTools/tests/test_docs_angelscript_style.py`) точно так же вызывает `buildtools.format_files(..., ["**/*.fos"])`, а другие скрипты BuildTools (`package.py`, `docs_support_matrix.py`) уже импортируют этот модуль. Эти функции являются модульными вспомогательными функциями, а не документированным интерфейсом командной строки, поэтому закрепите ревизию движка, держите обёртку небольшой и покройте её тестом, который форматирует nullable-пример:

```python
import sys
from pathlib import Path

project_root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(project_root / 'Engine' / 'BuildTools'))

import buildtools

clang_format = buildtools.discover_clang_format()
changed = buildtools.format_files(clang_format, project_root / 'Scripts', ['**/*.fos'])
print(f'formatted {changed} file(s)')
```

Обёртка переписывает файлы на месте. Её проверка расхождений — пара «запустить форматтер, затем `git diff --exit-code`»; так же CI движка использует `format-source`. Проект, который заново реализует исправление вместо импорта, обязан поддерживать его эквивалентным `fix_fos_nullable_suffix`; комментарий в исходнике `buildtools.py` называет именно этот риск дублирования.

### Managed C# в игре

Движок форматирует собственный C# через clang-format, но `format-source` не заходит в `Scripts/*.cs` игры. К C# проекта применимы два других механизма:

- Сгенерированный проект скриптов всегда задаёт `EnforceCodeStyleInBuild=true`, поэтому правила стиля `IDE*` выполняются при сборке с уровнем серьёзности, назначенным в `.editorconfig`. `ManagedScriptBaker` включает каждый `.editorconfig`, найденный выше исходного файла, в свой инкрементальный штамп, поэтому смена уровня серьёзности приводит к перекомпиляции скриптов.
- Движок не поставляет `.editorconfig` для игр. Проект пишет его сам и хранит в своём репозитории.

Форматировать ли C# проекта ещё и через clang-format с помощью `buildtools.format_files` и собственного `.clang-format` — выбор проекта; движок документирует этот механизм только для своих исходников.

## Предупреждения считаются сбоями

- Managed-скрипты: сгенерированный проект задаёт `TreatWarningsAsErrors=true`. Поэтому повышение правила анализатора с `suggestion` до `warning` превращает его в сбой сборки; вводите правила по одному шагу серьёзности. `ManagedScript.AnalysisLevel` и `ManagedScript.AnalysisMode` выбирают профиль анализа SDK; если задан любой из них, проект также включает `EnableNETAnalyzers`.
- AngelScript: компилируйте каждую роль, до которой может дотянуться изменение (`SERVER`, `CLIENT`, `MAPPER`), через `CompileAngelScript`. Backend печатает диагностику в виде `<file>(<line>,<col>): warning : <text>`. В исходниках движка, просмотренных для этой страницы, нет переключателя, который превращает эти строки в ненулевой код завершения, поэтому проект, считающий их сбоями, проверяет вывод шага компиляции на этот шаблон.
- Нативный код: движок собирается с MSVC `/W4` без `/WX` и считает предупреждение дефектом по политике, а не по флагу компилятора. Проект делает собственные цели строгими так, как это делает `Examples/NativeExtensionSample/CMakeLists.txt`:

```cmake
if(MSVC)
    target_compile_options(NativeExtensionCore PRIVATE /W4 /WX)
else()
    target_compile_options(NativeExtensionCore PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
```

Применяйте такие флаги к библиотекам, принадлежащим проекту, а не к вложенным сторонним целям. Сборки Clang дополнительно падают на диагностике потокобезопасности для кода движка и проекта; MSVC и GCC этот анализ не запускают, поэтому оставьте хотя бы одну линию на Clang. См. [анализ потокобезопасности](../../contributing/coding-contracts/thread-safety-analysis.md).

## Выбор анализа, доступного проекту

| Анализ | Что предоставляет движок | Что должен предоставить проект |
|---|---|---|
| Nullability | Принудительная проверка компилятором и в runtime в обоих backend; Managed-диагностика nullable через сгенерированный проект; `check_script_bindings.py` сравнивает обработчики событий и удалённых вызовов AngelScript с их объявлениями ([проверки скриптов](script-and-binding-checks.md)) | Запуск проверки на областях проекта; другие правила авторинга; см. [Nullability](../../contributing/coding-contracts/nullability.md) |
| Покрытие сущностей (Managed) | `FOSYNC001`-`FOSYNC005`, `FOSYNC009`-`FOSYNC011`, `FOSYNC015` в `FOnline.Analyzers.csproj`; `FOSYNC006`, `FOSYNC007` и `FOSYNC012`-`FOSYNC014` выведены из употребления | Добавить ссылку на проект (ниже), задать уровни серьёзности в `.editorconfig`, понизить правила для тестовых исходников отдельной секцией `.editorconfig` по пути |
| Контракты движка (Managed) | `FOENTRY001`-`FOENTRY003`, `FOVERIFY001`-`FOVERIFY003` и `FOSTATIC001` в том же проекте анализаторов ([анализаторы контрактов движка](engine-contract-analyzers.md)) | Уровни серьёзности, параметры `fonline_entry.*` для атрибутов проекта, приёмочный probe |
| Покрытие сущностей (нативный код) | `native_safety_audit.py sync-scope` со встроенными правилами движка ([аудит безопасности нативного кода](native-safety-audits.md)) | `--scope` для нативных каталогов проекта, проверенные планировщики и фабрики проекта в конфигурации, baseline с причинами |
| Потокобезопасность (нативный код) | Макросы `FO_TSA_*` и флаг Clang | Аннотации в блокировках проекта, перечень исключений |
| Выделение памяти | Аудит `allocator` из `native_source_audit.py` ([аудит нативного кода](native-source-audits.md)) | Области, исключения для vendored-кода, baseline проекта |
| Локальные переменные | Правила, маркеры `FO_REDUNDANT_CONST_SUPPRESS` и `FO_USE_AFTER_MOVE_SUPPRESS` и аудиты `local-variables` и `explicit-local-types` ([аудит нативного кода](native-source-audits.md)) | Compile database, собранная после генерации кода и урезанная до одной команды на единицу трансляции; `clang-query` и `clang-tidy` 20+ на хосте; `--require-tooling` в линии Clang |
| Политика указателей | Правила, тесты обёрток и аудиты `smart-pointer` и `smart-pointer-ast` с точными строками baseline ([аудит нативного кода](native-source-audits.md)) | Baseline проекта с причиной в каждой строке; ABI-шаблоны и nullable-фабрики проекта в конфигурации |
| Безопасность исключений | Словарь классификации, правила вывода и `native_safety_audit.py exception-safety` с baseline движка ([аудит безопасности нативного кода](native-safety-audits.md)) | Baseline проекта для его нативных областей, заново выводимый при каждом изменении |
| Раскладка исходников и комментарии | `check_native_source_layout.py --root <dir> --source-dir <dir>`; `--comments` с baseline комментариев по счётчикам | Запуск над `SourceExt` |
| Гигиена модулей AngelScript | `check_angelscript_hygiene.py` с baseline-храповиком по счётчикам ([проверки скриптов](script-and-binding-checks.md)) | Конфигурация, включаемые правила и пороги вроде `file-size`, правила только для проекта в отдельном скрипте |
| Даты временной совместимости | `temporary_compat.py <dirs>` | Передача каталогов проекта |
| Санитайзеры | Конфигурации `San_Address`, `San_Memory`, `San_Undefined`, `San_Thread` | Presets и линия на Linux; сценарии `validate.sh unit-tests-san-*` запускают собственный проект валидации движка |

Подключите Managed-анализатор через главную конфигурацию (путь относительно корня проекта, как в [руководстве по конфигурации проекта](../build/project-configuration.md)):

```ini
ManagedScript.Analyzers = Engine/Source/Scripting/Managed/Analyzers/FOnline.Analyzers.csproj
ManagedScript.AnalyzerPackages = <PackageId>,<exact-version>
ManagedScript.AdditionalFiles = <analyzer-config-file>
```

Версии пакетов должны быть точными; записи, содержащие в версии `*`, `[`, `]`, `(`, `)`, `<`, `>`, `=` или `,`, отклоняются, потому что плавающая версия делает набор правил зависящим от дня сборки. Изменение проекта анализатора, дополнительного файла или `.editorconfig` принудительно перекомпилирует скрипты, поэтому новое правило не может молчать за устаревшим инкрементальным результатом.

Две проверки движка принимают дерево проекта напрямую. `check_native_source_layout.py` принимает корень проекта и исходный каталог относительно корня; с `--root Examples/MinimalMultiplayer --source-dir SourceExt` проверка проходит. Она требует, чтобы первым включением был `Common.h`, и ровно один завершающий перевод строки, поэтому намеренно автономный файл библиотеки будет отмечен. `check_native_modules.py` читает списки исходников CMake движка и не является инструментом проекта.

## Проверка сгенерированного вывода вместо его правки

Сгенерированный файл никогда не является объектом исправления; исправьте его вход или генератор и сгенерируйте заново. Дайте каждому генератору режим проверки, который падает, когда закоммиченный вывод устарел, и запускайте его до запекания или компиляции:

- Генераторы движка следуют шаблону: `--write` создаёт, `--check` проверяет. `BuildTools/docs_metadata.py --metadata <server> --metadata <client> --check` сверяет каталог удалённых вызовов проекта с запечёнными метаданными (см. [сгенерированные данные](../build/generated-content.md)).
- `Examples/MinimalMultiplayer/generate_config.py --check` проверяет сгенерированный `.fomain`, а его `CMakeLists.txt` делает эту проверку зависимостью `BakeResources` и `ForceBakeResources`. Генератор проекта должен следовать этой форме, чтобы устаревший вывод останавливал запекание, а не попадал в него.
- После повторной генерации `git diff --exit-code` — нейтральная проверка расхождений для любого вывода, который инструмент записывает на месте.

## Честные baseline и исключения

Baseline фиксирует известные нарушения; файл исключений фиксирует намеренные исключения. Оба остаются достоверными при соблюдении четырёх правил:

1. У каждой строки есть письменная непустая причина. Образец — `BuildTools/NativeModuleExceptions.json` движка: каждая запись сопоставляет путь с предложением, и `check_native_modules.py` отклоняет пустую причину. Подавления в исходном коде следуют той же идее; маркеры локальных переменных требуют причину не короче восьми символов.
2. Устаревшая строка приводит к сбою. Когда коду исключение больше не нужно, проверка сообщает о строке, чтобы её удалили (`check_native_modules.py` падает с сообщением "is stale").
3. Baseline только сокращается. Точные записи на уровне строк или идентичностей предпочтительнее счётчиков, потому что бюджет в виде счётчика позволяет одному новому нарушению спрятаться за одним исправленным. Руководство по умным указателям высказывает это для allowlist.
4. Проверку никогда не ослабляют, не обходят и не заносят в baseline, чтобы получить зелёный прогон. Исправьте проблемного владельца либо добавьте рассмотренное исключение с причиной и владельцем, который его удалит. Собственные инструкции движка формулируют то же правило для сборок и кодогенерации.

## Порядок агрегирующего маршрута

Запускайте проверки в порядке, в котором каждая производит то, что потребляет следующая:

1. Generate: заново сгенерируйте выводы, принадлежащие проекту, затем переконфигурируйте, если менялись входы CMake.
2. Format: `buildtools.py format-content --check` (с `--fos` для AngelScript) и форматтеры проекта в режиме проверки, либо запустите их и затем `git diff --exit-code` ([Форматирование файлов игрового контента](format-content.md)).
3. Analyze: статические проверки, не требующие запекания, в таком порядке:
   1. `check_native_source_layout.py` и `temporary_compat.py` на каталогах проекта;
   2. `native_source_audit.py allocator` и `smart-pointer` ([аудит нативного кода](native-source-audits.md));
   3. `native_safety_audit.py sync-scope` и `exception-safety` ([аудит безопасности нативного кода](native-safety-audits.md));
   4. `check_script_bindings.py` и `check_angelscript_hygiene.py` ([проверки скриптов](script-and-binding-checks.md));
   5. `check_text_packs.py` ([текстовые пакеты](content-and-diagnostics-tools.md#проверка-текстовых-пакетов-до-бейкинга)) и `migration_guard.py --check` ([защита миграций](migration-guard.md));
   6. `markdown_links.py` по Markdown проекта ([проверка документации](documentation-checks.md));
   7. аудиты только для проекта;
   8. после configure и генерации кода, в линии с Clang 20: аудиты `local-variables`, `explicit-local-types` и `smart-pointer-ast` с `--require-tooling`.
4. Validate: скомпилируйте скрипты для каждой роли с включёнными Managed-[анализаторами контрактов движка](engine-contract-analyzers.md) и выполните запекание плюс валидаторы контента.
5. Test: нативные модульные тесты, затем тесты скриптов через [обвязку скриптовых тестов](script-test-harness.md) и тесты уровня процессов (см. [Добавление и запуск тестов в игровом проекте](project-tests.md)).

Порядок сгенерированных данных и последовательность configure/compile/bake описаны в разделе [сгенерированные данные](../build/generated-content.md). Не запускайте запекание одновременно с потребителем его результата.

## Размещение каждой проверки

| Проверка | Pre-commit | Локальная агрегирующая задача | CI |
|---|---|---|---|
| Форматтер в режиме записи | Да, на staged-файлах, с повторным добавлением результатов в индекс | Да | Нет; только режим проверки |
| Проверка расхождений форматтера | Нет | Да | Да |
| Генератор `--check` | Да, если быстрый | Да | Да |
| Раскладка, `temporary_compat.py` | Да | Да | Да |
| Текстовые аудиты нативного кода, аудиты безопасности нативного кода, проверки скриптовых привязок и гигиены | Да, если менялись нативные или скриптовые файлы | Да | Да |
| Проверка текстовых пакетов, защита миграций, проверка ссылок документации | Да, если быстрые | Да | Да |
| Аудиты нативного кода на основе Clang | Нет | По требованию | Да, в одной линии Clang с `--require-tooling` |
| Компиляция скриптов, все роли | Нет | Да | Да |
| Запекание и валидация контента | Нет | Да | Да |
| Нативные модульные тесты | Нет | Да | Да |
| Игровые тесты уровня процессов | Нет | Необязательно | Да |
| Санитайзеры, долгие анализаторы, линии пакетов | Нет | По требованию | Да, по расписанию или по классу изменений |

Движок не поставляет Git hook, поэтому pre-commit hook — файл проекта (например, выбранный через `git config core.hooksPath`), который вызывает те же команды, что и агрегирующая задача. Держите hooks быстрыми и никогда не полагайтесь на них: CI повторяет каждую проверку, потому что hook можно пропустить. Определите агрегирующую задачу в рабочем пространстве, как описано в [Настройка рабочего пространства VS Code для проекта FOnline](vscode-workspace.md), и повторите её шаги в конвейере из [Настройка непрерывной интеграции для игрового проекта](continuous-integration.md).

## Контрольный список проверки

- `FO_CLANG_FORMAT` указывает на один закреплённый бинарный файл clang-format 20 на каждой рабочей станции и каждом runner.
- `.fos`-файлы проекта форматируются через исправление с учётом nullable, никогда не необработанным clang-format, и тест покрывает пример с `T?`.
- `.editorconfig` и настройки анализаторов закоммичены, а после смены уровня серьёзности выполнена перекомпиляция.
- Нативные цели проекта собираются с предупреждениями как ошибками на MSVC и на одной линии Clang.
- У каждого генератора есть режим `--check`, подключённый до потребителя.
- У каждой строки baseline и исключений есть причина; проверка падает на устаревших строках.
- Аудиты и проверки движка запускаются с областями, конфигурацией и baseline-файлами проекта через тонкие обёртки, и в проекте не осталось копий проверок движка.
- Агрегирующая задача выполняет generate, format, analyze, validate и test в этом порядке, и CI повторяет её.
