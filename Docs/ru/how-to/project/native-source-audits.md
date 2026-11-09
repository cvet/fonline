---
layout: default
title: Аудит правил нативного кода в игровом проекте
locale: ru
document_id: project-native-source-audits
permalink: /Docs/ru/how-to/project/native-source-audits.html
---

# Аудит правил нативного кода в игровом проекте
<!-- docs-translation: {"document_id":"project-native-source-audits","locale":"ru","source_path":"Docs/en/how-to/project/native-source-audits.md","source_sha256":"277d2192d9f02a0caa12224295bc7526f7c9c1e0cfd5af54833a2cc986de3201"} -->
`BuildTools/native_source_audit.py` проверяет нативные исходники C++ на соответствие контрактам кода движка для выделения памяти, умных указателей и локальных переменных. Движок запускает его на собственном `Source/` с baseline, которым владеет движок; игровой проект запускает ту же команду на своих каталогах нативных расширений со своими областями проверки и своим baseline и держит у себя только тонкую обёртку. На странице описаны команды, форматы конфигурации и baseline, а также расширение аудитов в проекте без их копирования.

## Проверенные исходные пути

- `AGENTS.md`
- `BuildTools/native_source_audit.py`
- `BuildTools/native_source_audit/cli.py`
- `BuildTools/native_source_audit/common.py`
- `BuildTools/native_source_audit/allocator.py`
- `BuildTools/native_source_audit/smart_pointer.py`
- `BuildTools/native_source_audit/raw_pointer_ast.py`
- `BuildTools/native_source_audit/local_variables.py`
- `BuildTools/native_source_audit/explicit_local_types.py`
- `BuildTools/native_source_audit/clang_support.py`
- `BuildTools/native_source_audit/clang_audits.py`
- `BuildTools/native_source_audit/queries/redundant_local_const.query.in`
- `BuildTools/native_source_audit/queries/explicit_local_types.query.in`
- `BuildTools/audit_baselines/allocator.tsv`
- `BuildTools/audit_baselines/smart-pointer.tsv`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/tests/test_native_source_audit.py`
- `BuildTools/tests/test_native_source_audit_smart_pointer.py`
- `BuildTools/tests/test_native_source_audit_clang.py`
- `Source/Essentials/Containers.h`
- `Source/Essentials/MemorySystem.h`
- `Source/Essentials/SmartPointers.h`
- `Docs/en/contributing/coding-contracts/smart-pointers.md`
- `Docs/en/contributing/coding-contracts/local-variables.md`
- `Docs/en/contributing/coding-contracts/exception-safety.md`
- `Docs/en/contributing/coding-contracts/nullability.md`
- `Docs/en/reference/native/essentials.md`
- `Docs/en/how-to/project/quality-gates.md`

## Что проверяет каждый аудит

| Аудит | Правило движка, которое он проверяет | Требования |
|---|---|---|
| `allocator` | Код движка выделяет память через словарь системы памяти: никаких контейнеров `std::`, `std::format`/`std::to_string` и голых вызовов семейства `malloc`/`free` вне проверенных границ ([контракт нехватки памяти](../../contributing/coding-contracts/exception-safety.md#1-нехватка-памяти-не-является-восстанавливаемой-ошибкой), [словарь выделения памяти](../../reference/native/essentials.md#словарь-выделения-памяти), раздел "Memory/callables/platform" в `AGENTS.md`) | Только Python |
| `smart-pointer` | Заимствования `ptr`/`nptr`, владельцы `unique_*`/`refcount_*`, никаких владельцев `std::`, никаких полей-ссылок, проверка nullable перед разыменованием, сырые указатели только в распознанных формах ABI ([контракты указателей](../../contributing/coding-contracts/smart-pointers.md#контракты), [allowlist сырых указателей](../../contributing/coding-contracts/smart-pointers.md#allowlist-raw-pointer), [граница script binding](../../contributing/coding-contracts/smart-pointers.md#граница-script-binding-fo_script_api), [nullability](../../contributing/coding-contracts/nullability.md)) | Только Python |
| `smart-pointer-ast` | То же правило сырых указателей по AST: сырые указатели в переменных, полях, параметрах и возвращаемых значениях на добавленных строках | `clang-query` из Clang 20+, `compile_commands.json` |
| `local-variables` | [Избыточный локальный `const`](../../contributing/coding-contracts/local-variables.md#избыточный-локальный-const) и [использование после перемещения](../../contributing/coding-contracts/local-variables.md#использование-после-перемещения) | `clang-query` и `clang-tidy` из Clang 20+, `compile_commands.json` |
| `explicit-local-types` | [Явные простые локальные типы](../../contributing/coding-contracts/local-variables.md#явные-простые-локальные-типы) вместо `auto` | `clang-query` из Clang 20+, `compile_commands.json` |

Раскладка классов, размещение `[[nodiscard]]`, инициализация фигурными скобками и группировка пустыми строками этими аудитами не проверяются и остаются правилами ревью. Текстовые аудиты проверяют устойчивые синтаксические маркеры и не заменяют компиляцию.

Запускайте оба текстовых аудита перед каждым коммитом, затрагивающим нативный код, и в каждом прогоне CI. Аудиты на основе Clang запускайте там, где есть база компиляции, как минимум в одной линии CI.

## Запуск аудитов

В отдельном checkout движка область проверки по умолчанию — `Source`. Windows (PowerShell):

```powershell
py -3 BuildTools\native_source_audit.py allocator
py -3 BuildTools\native_source_audit.py smart-pointer
```

Linux:

```bash
python3 BuildTools/native_source_audit.py allocator
python3 BuildTools/native_source_audit.py smart-pointer
```

Из корня игрового проекта с движком в `Engine/` область по умолчанию по-прежнему `Source` движка; каталоги проекта передайте явно или через файл конфигурации:

```powershell
py -3 Engine\BuildTools\native_source_audit.py allocator --scope Engine/Source --scope SourceExt
py -3 Engine\BuildTools\native_source_audit.py smart-pointer --config Tools/NativeAudits/native-audits.json
```

```bash
python3 Engine/BuildTools/native_source_audit.py allocator --scope Engine/Source --scope SourceExt
python3 Engine/BuildTools/native_source_audit.py smart-pointer --config Tools/NativeAudits/native-audits.json
```

Аудитам на основе Clang нужен каталог с `compile_commands.json`, например сборка CMake, настроенная с `CMAKE_EXPORT_COMPILE_COMMANDS=ON` и компилятором Clang, после того как генерация кода создала сгенерированные заголовки:

```bash
python3 Engine/BuildTools/native_source_audit.py local-variables --scope SourceExt --compilation-database Build/ClangAnalysis
python3 Engine/BuildTools/native_source_audit.py explicit-local-types --scope SourceExt --compilation-database Build/ClangAnalysis
python3 Engine/BuildTools/native_source_audit.py smart-pointer-ast --scope SourceExt --compilation-database Build/ClangAnalysis --diff-base origin/main
```

Без `clang-query`/`clang-tidy` (они ищутся через `--clang-query`, `--clang-tidy`, `FO_CLANG_QUERY`, `FO_CLANG_TIDY` или `PATH`) или без базы компиляции эти аудиты печатают `skipped: <reason>` и завершаются с кодом 0. В линии CI, где установлен Clang, добавьте `--require-tooling`, чтобы отсутствие инструмента приводило к сбою. `smart-pointer-ast --diff-base` сравнивает один git-репозиторий; запускайте его из репозитория, которому принадлежат области проверки, потому что файлы движка в подмодуле не входят в diff проекта.

Полезные параметры, общие для всех аудитов:

| Параметр | Действие |
|---|---|
| `--root` | Каталог, относительно которого заданы области проверки, шаблоны исключения, отчёты, конфигурация и baseline проекта; по умолчанию текущий каталог |
| `--scope` | Файл или каталог для проверки; можно повторять; заменяет `scopes` из конфигурации |
| `--exclude` | Шаблон fnmatch для путей относительно корня; `*` проходит и через `/`; шаблон, оканчивающийся на `/`, пропускает поддерево; можно повторять, добавляется к списку из конфигурации |
| `--baseline` | Дополнительный baseline проекта; можно повторять |
| `--no-engine-baseline` | Показывает все находки движка, включая проверенные |
| `--report` | Записывает оставшиеся находки и устаревшие строки в TSV |
| `--emit-baseline` | Записывает строки-кандидаты baseline для оставшихся находок с пустой колонкой причины |
| `--fail-on` | Только текстовые аудиты: `error` (по умолчанию), `warning` или `none` |
| `--mode` | Аудиты Clang: `check` (по умолчанию) падает на любой активной находке, `report` никогда не падает, `apply` переписывает исходники (`local-variables` требует `--checks redundant-local-const`) |

Точный список параметров приведён в сгенерированном [справочнике helper CLI](../../reference/helper-cli/index.md).

## Коды завершения

| Код | Значение |
|---|---|
| 0 | Нет находок, приводящих к сбою, и нет устаревших строк baseline, либо аудит на основе Clang пропущен из-за отсутствия инструментов |
| 1 | Есть хотя бы одна находка на пороге сбоя или выше либо устаревшая строка baseline |
| 2 | Неверный вызов, конфигурация или baseline, область проверки вне `--root`, отсутствующая область, сбой инструмента Clang или отсутствие инструментов при `--require-tooling` |

## Настройка областей и параметров

Файл конфигурации выносит решения проекта из командных строк. Все пути в нём заданы относительно `--root`, а неизвестные ключи приводят к ошибке:

```json
{
  "schema_version": 1,
  "scopes": ["Engine/Source", "SourceExt"],
  "exclude": ["SourceExt/vendor-*/"],
  "baselines": {
    "allocator": ["Tools/NativeAudits/allocator.tsv"],
    "smart-pointer": ["Tools/NativeAudits/smart-pointer.tsv"]
  },
  "compilation_database": "Build/ClangAnalysis",
  "smart_pointer": {
    "raw_return_guarded_files": ["SourceExt/JsonBridge.cpp"],
    "abi_line_patterns": ["\\bProjectSdkCallback\\b"],
    "nullable_factory_names": ["projectsdk_open_[A-Za-z0-9_]*"]
  }
}
```

- `scopes` и `exclude` выбирают файлы; вендорные и сторонние деревья исключайте здесь, а не заносите в baseline.
- `baselines` сопоставляет имени аудита файлы baseline проекта; каждый указанный файл должен существовать (файл из одной строки заголовка допустим).
- `compilation_database` — каталог с `compile_commands.json` для аудитов на основе Clang.
- `smart_pointer.raw_return_guarded_files` добавляет файлы проекта, в которых прямой `return wrapper.get();` должен проходить через именованную границу `Return*`/`BorrowScript*`, как в файлах script-методов движка.
- `smart_pointer.abi_line_patterns` добавляет регулярные выражения для форм ABI проекта (callback-функции, экспортируемые точки входа C); подходящие строки с сырыми указателями классифицируются как маркеры ABI, а не как обычные сырые указатели.
- `smart_pointer.nullable_factory_names` добавляет имена nullable-фабрик C из библиотек проекта; прямое принятие их результата в non-null `unique_del_ptr<T>` приводит к сбою.

Правила, которые называют файлы движка (например, владельцы аллокатора `Source/Essentials/Containers.h` и `MemorySystem.*` или проверенные контракты загрузки `optional<refcount_ptr<T>>`), применяются только к файлам внутри каталога `Source` движка. Любой другой файл считается кодом проекта и подчиняется строгим правилам стиля null и проверке разнесённой реализации script-функций через `FO_NAMESPACE`.

## Запись и ревью baseline

Baseline — файл TSV в UTF-8 с фиксированным заголовком и четырьмя непустыми колонками:

```text
path	rule	code	reason
SourceExt/JsonBridge.cpp	StdContainerType	return std::string {"name"};	nlohmann::json returns std::string; converted at this boundary only.
SourceExt/Json/	StdContainerType	*	Translation layer whose whole job is building nlohmann::json values.
```

- `path` — файл или каталог, оканчивающийся на `/`, который покрывает всё поддерево. Baseline проекта задаётся относительно `--root`; baseline движка `BuildTools/audit_baselines/<audit>.tsv` задаётся относительно корня движка и пересчитывается, поэтому он действует и тогда, когда движок проверяется как `Engine/Source`.
- `rule` — имя правила из находки.
- `code` — строка исходника со схлопнутыми пробелами; она сопоставляется независимо от номера строки, поэтому посторонние правки не делают строку недействительной; `*` принимает любую строку этого правила в данном пути.
- `reason` объясняет, почему это место корректно. Строка без причины отклоняется.

Строка внутри проверяемых областей, которой не соответствует ни одна находка, сообщается как `StaleBaselineEntry` и приводит к сбою, поэтому исправленное место удаляет свою строку в том же изменении. Строки вне текущих областей не проверяются, поэтому проект может проверять только `SourceExt`, не спотыкаясь о строки движка.

Чтобы добавить строку, сначала изучите код за находкой, затем создайте кандидата и напишите причину:

```bash
python3 Engine/BuildTools/native_source_audit.py allocator --config Tools/NativeAudits/native-audits.json --emit-baseline Workspace/allocator-candidates.tsv
```

Переносите в baseline проекта только проверенные строки и заполняйте колонку причины. Предпочитайте точные строки `code`; `*` и строки каталогов используйте для целого слоя трансляции, назначение которого — сама граница. Аудиты Clang принимают также маркеры в исходнике `FO_REDUNDANT_CONST_SUPPRESS: <reason>` и `FO_USE_AFTER_MOVE_SUPPRESS: <reason>`, описанные в [локальных переменных](../../contributing/coding-contracts/local-variables.md#использование-после-перемещения).

## Подключение к локальной задаче и CI

Вызывайте одну и ту же команду или обёртку проекта, показанную ниже, из локальной агрегирующей задачи и из CI на этапе анализа, описанном в [проверках качества](quality-gates.md#порядок-агрегирующего-маршрута). Текстовым аудитам не нужна сборка, поэтому они выполняются рано на любом runner с Python 3.12+. Аудиты на основе Clang ставьте после конфигурации и генерации кода в линии, где есть Clang 20+, с `--require-tooling`. Сохраняйте вывод `--report` как артефакт CI, если прогон упал.

## Расширение в проекте

Держите в проекте только данные проекта и тонкую обёртку; не копируйте код аудитов.

1. Добавьте `Tools/NativeAudits/native-audits.json` (имя выбирает проект) с областями проверки, шаблонами исключения, baseline и параметрами smart-pointer, как показано выше.
2. Рядом добавьте по одному baseline на аудит, каждый начинается со строки заголовка; у каждой строки причина записана в той же строке, поэтому ревьюер видит их вместе.
3. Добавьте обёртку, которая вызывает инструмент движка для каждого аудита и возвращает худший код завершения:

```python
"""Run the Engine native source audits with this project's scopes and baselines."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENGINE_TOOL = ROOT / 'Engine' / 'BuildTools' / 'native_source_audit.py'
CONFIG = 'Tools/NativeAudits/native-audits.json'
AUDITS = ('allocator', 'smart-pointer', 'local-variables', 'explicit-local-types')


def main() -> int:
    worst = 0

    for audit in AUDITS:
        command = [sys.executable, str(ENGINE_TOOL), audit, '--root', str(ROOT), '--config', CONFIG, *sys.argv[1:]]
        worst = max(worst, subprocess.run(command, check=False).returncode)

    return worst


if __name__ == '__main__':
    sys.exit(main())
```

4. Правила, специфичные для проекта (например, запрет API, существующего только в игре), помещайте в отдельный проверяющий скрипт проекта рядом с обёрткой, со своими тестами. Не добавляйте их в аудит движка и не прячьте находку по правилу движка в проверке проекта.
5. Проверка `Engine/Source` из проекта необязательна: CI движка уже запускает текстовые аудиты с baseline движка на каждой ревизии движка. Оставьте её, если проекту нужно локальное подтверждение после обновления движка.

Проект, который держал собственную раннюю копию этих аудитов, переходит за одно изменение: перенесите строки о файлах проекта из старых файлов исключений или allowlist в новые baseline из четырёх колонок (одна строка на правило, `*` вместо прежних исключений целых файлов, старая классификация включается в причину), удалите строки о файлах движка, потому что ими владеет baseline движка, перенесите префиксы вендорных каталогов в `exclude`, замените старые команды в задачах и CI на обёртку, удалите скопированные инструменты и их self-test, а затем запускайте обёртку, пока она не завершится с кодом 0 без устаревших строк.

## Контрольный список проверки

- `allocator` и `smart-pointer` завершаются с кодом 0 на областях проекта с baseline проекта и на `Source` движка с baseline движка.
- У каждой строки baseline есть причина, и `StaleBaselineEntry` не сообщается.
- Вендорные и сторонние каталоги исключены в конфигурации, а не занесены в baseline.
- Аудиты на основе Clang выполняются как минимум в одной линии CI с `--require-tooling`, после генерации кода, на `compile_commands.json` с одним реальным разбором на единицу трансляции.
- `smart-pointer-ast --diff-base` запускается из репозитория, которому принадлежат проверяемые файлы.
- Обёртка возвращает ненулевой код, если падает любой аудит, а локальная задача и CI вызывают одну и ту же обёртку.
