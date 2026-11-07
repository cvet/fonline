---
layout: default
title: Проверка скриптовых привязок и гигиены AngelScript
locale: ru
document_id: project-script-and-binding-checks
permalink: /Docs/ru/how-to/project/script-and-binding-checks.html
---

# Проверка скриптовых привязок и гигиены AngelScript
<!-- docs-translation: {"document_id":"project-script-and-binding-checks","locale":"ru","source_path":"Docs/en/how-to/project/script-and-binding-checks.md","source_sha256":"PLACEHOLDER"} -->
Две проверки движка, работающие только на чтение, находят ошибки на границе скриптов
раньше сборки или бейка. `check_script_bindings.py` удерживает обращённые к скриптам
нативные сигнатуры в словаре `ptr<T>`/`nptr<T>` и сравнивает обработчики AngelScript
с объявлениями событий и удалённых вызовов, которые они реализуют.
`check_angelscript_hygiene.py` проверяет соглашения о модулях, на которые опирается
бэкенд AngelScript. Обе принимают корень проекта, области проекта и baseline с
храповиком счётчиков, поэтому игровой проект запускает инструменты движка вместо
собственных копий.

## Проверенные исходные пути

- `BuildTools/check_script_bindings.py`
- `BuildTools/check_angelscript_hygiene.py`
- `BuildTools/angelscript_source.py`
- `BuildTools/finding_baseline.py`
- `BuildTools/check_native_source_layout.py`
- `BuildTools/codegen.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/tests/test_check_script_bindings.py`
- `BuildTools/tests/test_check_angelscript_hygiene.py`
- `BuildTools/tests/test_finding_baseline.py`
- `Source/Scripting/AngelScript/AngelScriptBackend.cpp`
- `Source/Scripting/AngelScript/AngelScriptEntity.cpp`
- `Source/Scripting/AngelScript/AngelScriptHelpers.cpp`
- `Source/Scripting/AngelScript/AngelScriptRemoteCalls.cpp`
- `Source/Server/Server.h`
- `ThirdParty/AngelScript/sdk/angelscript/source/as_datatype.cpp`
- `Examples/MinimalMultiplayer/Scripts/Tutorial.fos`
- `Docs/en/contributing/coding-contracts/nullability.md`
- `Docs/en/how-to/scripting/style-and-refactoring.md`

## Когда их запускать

| Проверка | Что она доказывает | Когда запускать |
| --- | --- | --- |
| `check_script_bindings.py` | Сырой `T*` не попадает в обращённую к скриптам нативную сигнатуру; сохранённые обратные вызовы сохраняют экспортированную сигнатуру; обработчики AngelScript согласны с объявлениями насчёт `null` | После правки `SourceExt`, объявлений `///@ Event` или `///@ RemoteCall` либо сигнатур обработчиков; в CI на каждом изменении |
| `check_angelscript_hygiene.py` | Соглашения модуля о namespace, include, макросах сторон, директиве сортировки и окончании файла | После добавления, переноса или разделения файлов `.fos`; в CI на каждом изменении |

Ни одна из проверок не заменяет компиляцию. Кодогенерация по-прежнему отвергает сырые
указатели на handle при сборке, а компилятор AngelScript отвергает `?` на типах-значениях
и явный `@` на типах с неявным handle, поэтому эти два скриптовых правила здесь не
повторяются. Правилами владеют [контракт nullability](../../contributing/coding-contracts/nullability.md)
и [руководство по стилю AngelScript](../scripting/style-and-refactoring.md); инструменты
лишь сообщают о нарушениях раньше.

## Проверка скриптовых привязок

### Правила

| Правило | Что сообщает |
| --- | --- |
| `raw-pointer` | Сырой `T*` в сигнатуре `///@ ExportMethod` или другой `FO_SCRIPT_API`, в члене из списка `///@ ExportRefType ... Export =`, в данных `FO_ENTITY_EVENT` или в аргументах шаблонов `ScriptFunc`/`FindFunc`/`CheckFunc`. Объявления `///@ EngineHook`, внутренние указатели и строки `#define` в контракт не входят. |
| `obsolete-nullable-marker` | Удалённый маркер `FO_NULLABLE` в коде. |
| `stored-callback-signature` | `ScriptFunc<...>`, созданный из `std::move(<parameter>)` в теле `FO_SCRIPT_API`, сигнатура которого отличается от экспортированного параметра обратного вызова, например экспортирован `nptr<Item>`, а сохранён `ptr<Item>`. |
| `event-handler-nullability` | Обработчик AngelScript, чьи маркеры `?` по аргументам расходятся с объявлением события. Объявления — нативные события `///@ ExportEvent` и теги `///@ Event`; обработчики — функции, переданные в `<target>.<Event>.Subscribe(...)`, и функции `[[Event]]` с именем события. Ведущий ненулевой параметр владельца у события сущности учитывается. |
| `remote-call-handler-nullability` | Функция `[[ServerRemoteCall]]` или `[[ClientRemoteCall]]`, чьи маркеры `?` расходятся с её объявлением `///@ RemoteCall`; неявный ведущий `Player` серверного обработчика учитывается. |

Правила для обработчиков важны, потому что компилятор AngelScript при сопоставлении
обработчика с funcdef события или с объявлением удалённого вызова сравнивает типы
handle, но не маркер `?`, и nullable-аргумент может попасть в параметр, который
обработчик считает ненулевым. Обработчик с другим числом аргументов или имя события,
объявленное дважды с разной nullability, пропускаются: компилятор или
неоднозначность лишают сравнение смысла. `--list-rules` печатает идентификаторы.

### Запуск

В checkout движка по умолчанию сканируется `Source`:

```powershell
py -3 BuildTools/check_script_bindings.py
py -3 BuildTools/check_script_bindings.py --scope Source --scope Examples --scripts Examples
```

```bash
python3 BuildTools/check_script_bindings.py
python3 BuildTools/check_script_bindings.py --scope Source --scope Examples --scripts Examples
```

В игровом проекте, который встраивает движок в `Engine/`, передайте корень проекта и
включите `Engine/Source`, чтобы при сравнении обработчиков были известны нативные
объявления событий:

```powershell
py -3 Engine/BuildTools/check_script_bindings.py --root . --scope Engine/Source --scope SourceExt --scripts Scripts --exclude 'SourceExt/ThirdParty/*'
```

```bash
python3 Engine/BuildTools/check_script_bindings.py --root . --scope Engine/Source --scope SourceExt --scripts Scripts --exclude 'SourceExt/ThirdParty/*'
```

Находка называет файл, строку, правило и объявление, с которым шло сравнение:

```text
SourceExt/ServerExtension.cpp:4: [raw-pointer] raw pointer 'Critter*' in an ExportMethod signature; use ptr<T> for a non-null or nptr<T> for a nullable script handle
Scripts/Main.fos:12: [event-handler-nullability] handler 'OnLogin' for event OnPlayerLogin disagrees with the declaration at Engine/Source/Server/Server.h:231: argument 2 'cr' is declared nullable but the handler takes non-null Critter
[ScriptBindings] 2 finding(s), 2 above the baseline, 0 stale baseline row(s)
```

Области и каталоги скриптов задаются относительно корня и должны существовать;
`--exclude` принимает шаблоны `fnmatch` по относительным POSIX-путям, где `*` проходит
и через каталоги. Нативные файлы — `.h`, `.hpp`, `.cpp`, `.cc` и `.cxx`; скриптовые —
`.fos`. Разбор лексический: комментарии и строковые литералы игнорируются, читается
каждая ветвь `#if`, а обработчики Managed C# остаются за сгенерированной
nullable-поверхностью и Roslyn.

## Проверка гигиены модулей AngelScript

### Правила

| Правило | По умолчанию | Что сообщает |
| --- | --- | --- |
| `namespace-matches-file` | вкл. | Первый namespace верхнего уровня отличается от имени файла без расширения, или модуль не объявляет namespace. Параметры: `exceptions` (каждое с `path`, `namespace`, `reason`) и `require_namespace`. |
| `no-include` | вкл. | Директива `#include`. Бэкенд уже включает каждый настроенный файл скрипта в один корневой модуль, поэтому межмодульный вызов называет владельца как `Namespace::Function()`. |
| `side-macro-value-test` | вкл. | `#ifdef`, `#ifndef` или `defined()` для `SERVER`, `CLIENT` или `MAPPER`. Каждый модуль стороны определяет все три как `0` или `1`, поэтому сторону выделяет только `#if SERVER`. Параметр: `macros`. |
| `preprocessor-balance` | вкл. | `#endif`, `#else` или `#elif` без открытого блока и незакрытый блок. |
| `sort-directive` | вкл. | `Sort N` на последующей строке, которую бэкенд игнорирует, и первая строка, где за `Sort ` не следует целое число: бэкенд читает такое значение как `0`. |
| `file-ending` | вкл. | Некорректный UTF-8, UTF-8 BOM, отсутствие завершающего перевода строки или лишние пустые строки в конце файла. |
| `commented-out-code` | выкл. | Строки `//`, похожие на отключённые операторы или вызовы. |
| `file-size` | выкл. | Модуль длиннее `max_lines` строк (по умолчанию 1500). |

Движок не документирует порядок объявлений внутри модуля, поэтому инструмент проверяет
только то упорядочивание на уровне файла, которое бэкенд действительно читает. Язык
комментариев, словарь имён и предметные правила остаются в проекте.

### Запуск

```powershell
py -3 BuildTools/check_angelscript_hygiene.py
py -3 Engine/BuildTools/check_angelscript_hygiene.py --root . --scripts Scripts --config Tools/QualityBaselines/angelscript_hygiene.json
```

```bash
python3 BuildTools/check_angelscript_hygiene.py
python3 Engine/BuildTools/check_angelscript_hygiene.py --root . --scripts Scripts --config Tools/QualityBaselines/angelscript_hygiene.json
```

Без `--scripts` инструмент проверяет `Examples` — именно это содержит checkout движка.
`--list-rules` печатает каждое правило с его значением по умолчанию.

### Конфигурация

Конфигурация — JSON-объект с `schema_version` `1`, необязательным списком `exclude`
из шаблонов `fnmatch` и объектом `rules`, где ключ — идентификатор правила. Каждое
правило принимает `enabled` и только собственные параметры. Неизвестные ключи,
правила или параметры и исключение без причины считаются ошибкой, поэтому опечатка
не может тихо отключить правило.

```json
{
  "schema_version": 1,
  "exclude": ["Scripts/Generated/*"],
  "rules": {
    "namespace-matches-file": {
      "exceptions": [
        {"path": "Scripts/ColorExt.fos", "namespace": "Color", "reason": "Extends the shared Color namespace instead of declaring its own."}
      ]
    },
    "no-include": {"enabled": false},
    "commented-out-code": {"enabled": true},
    "file-size": {"enabled": true, "max_lines": 1500}
  }
}
```

Сгенерированный вывод `.fos` исключайте здесь, а исправляйте его генератор; ручная
правка сгенерированного файла не является исправлением гигиены.

## Коды возврата

| Код | Значение |
| --- | --- |
| `0` | Нет находок сверх baseline и нет устаревших строк baseline. `--write-baseline` и `--list-rules` тоже завершаются с `0`. |
| `1` | Хотя бы одна находка сверх своего baseline или строка baseline, которая допускает больше находок, чем осталось. |
| `2` | Некорректный ввод: отсутствующая или выходящая за корень область, нечитаемый файл, неверная конфигурация или baseline, ошибка аргументов. |

## Baseline и храповик

Внедрение проверки в существующий код начинается с baseline, а не с ослабленного
правила. Baseline — UTF-8 файл с разделением табуляцией и заголовком `rule`, `path`,
`count`, `reason`; строки с `#` — комментарии. Строка допускает до `count` находок
одного правила в одном точном файле относительно корня:

```text
rule	path	count	reason
raw-pointer	SourceExt/ServerExtension.cpp	1	The ping bridge predates ptr<T>; convert it with the bridge rewrite.
side-macro-value-test	Scripts/Main.fos	1	Legacy server block, converted with the module split.
```

1. Запишите черновик: `--write-baseline Tools/QualityBaselines/angelscript_hygiene.tsv` фиксирует текущие счётчики с пустыми причинами.
2. Напишите причину в каждой строке. Загрузка отвергает пустую причину, поэтому необъяснённый черновик не может стать проверкой.
3. Передайте файл через `--baseline`. Падает и превышение допустимого числа находок, и строка, которая допускает больше, чем осталось, поэтому каждое исправление понижает или удаляет свою строку: счётчики только уменьшаются.

`--baseline` можно повторять. Держите отдельный файл на каждого владельца, например
файл движка и файл проекта рядом; пара `rule` и `path` может встречаться только в
одном из них. Файл из одного заголовка корректен и сохраняет командную строку
неизменной, когда все находки исправлены. Тот же формат обслуживает правила
нативных комментариев `check_native_source_layout.py --comments`, описанные в
[руководстве по дереву исходников](../../contributing/source-tree/index.md#проверка-нативных-комментариев).

## Подключение к локальной задаче и CI

Запускайте обе проверки в агрегирующей задаче качества проекта, после форматирования
и до компиляции, и в задании CI, которому нужны только Python 3.12 и checkout. Шаг CI
строится по такому образцу:

```text
- name: Check script bindings and AngelScript hygiene
  run: python3 Tools/check_scripts.py
```

Держите аргументы в одном скрипте проекта, как показано ниже, чтобы локальная задача
и CI не расходились. Порядок агрегирующего маршрута описан в
[проверках качества](quality-gates.md), настройка runner — в
[непрерывной интеграции](continuous-integration.md).

## Расширение в проекте

Инструменты движка остаются общими; проект добавляет вокруг них свою политику.

1. Добавьте тонкую обёртку, которая вызывает инструменты движка с корнем, областями, исключениями, конфигурацией и baseline проекта:

```python
"""Project quality gate: run the Engine script checks with this project's scopes and baselines."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE_TOOLS = ROOT / "Engine" / "BuildTools"
QUALITY = ROOT / "Tools" / "QualityBaselines"

CHECKS = [
    [ENGINE_TOOLS / "check_script_bindings.py", "--root", ROOT,
     "--scope", "Engine/Source", "--scope", "SourceExt", "--scripts", "Scripts",
     "--exclude", "SourceExt/ThirdParty/*", "--baseline", QUALITY / "script_bindings.tsv"],
    [ENGINE_TOOLS / "check_angelscript_hygiene.py", "--root", ROOT, "--scripts", "Scripts",
     "--config", QUALITY / "angelscript_hygiene.json", "--baseline", QUALITY / "angelscript_hygiene.tsv"],
    [ENGINE_TOOLS / "check_native_source_layout.py", "--root", ROOT, "--source-dir", "SourceExt", "--shallow",
     "--comments", "--comment-baseline", QUALITY / "native_comments.tsv"],
]


def main() -> int:
    failed = False

    for command in CHECKS:
        result = subprocess.run([sys.executable, *(str(part) for part in command)], check=False)
        failed = failed or result.returncode != 0

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
```

2. Правила, нужные только проекту, вынесите в отдельный скрипт проекта рядом с обёрткой и вызывайте из той же задачи: язык комментариев, словарь имён, выведенные из употребления игровые идентификаторы, обязательные сигнатуры обратных вызовов для атрибутов проекта или вспомогательные функции, дублирующие API движка. Используйте `BuildTools/angelscript_source.py` (классификация символов, маскирование кода, разбор функций и параметров) и `BuildTools/finding_baseline.py` (формат baseline) вместо копии лексера.
3. Держите каждую причину рядом с исключением: в столбце `reason` baseline или в `exceptions` для namespace в конфигурации. Причина называет владельца и условие, при котором строка удаляется.
4. Отключайте правило в конфигурации, только если собственный документ стиля проекта его не принимает, и фиксируйте это решение в том документе.

### Переход с прежней копии проекта

Проект, который держал собственный валидатор nullability или инструмент качества
скриптов, переходит на инструменты движка одним изменением:

1. Сопоставьте каждое старое правило с правилом движка. Нативные проверки сигнатур `ptr`/`nptr` становятся `raw-pointer` и `obsolete-nullable-marker`; паритет событий и удалённых вызовов — двумя правилами для обработчиков, которые теперь также следуют регистрациям `Subscribe(...)`; проверки namespace, баланса препроцессора, конца файла, закомментированного кода и размера файла — одноимёнными по смыслу правилами гигиены.
2. Правила без аналога в движке оставьте в скрипте проекта из шага 2 выше. Удалите правила, которые уже обеспечивает компилятор, например `?` на типах-значениях и явный `@` на типах с неявным handle.
3. Удалите скопированные валидаторы, их тесты и старые baseline, а также их шаги CI и задачи.
4. Запустите обёртку с `--write-baseline` для каждой проверки, впишите причину в каждую строку, затем запустите без этого параметра и убедитесь в коде возврата `0`.
5. Ожидайте новые находки там, где старая копия сопоставляла обработчики только по имени: это обработчики, зарегистрированные через `Subscribe(...)`. Исправьте их или занесите в baseline с причиной.

## Чек-лист проверки

- Обе проверки завершаются с `0` на проекте с закоммиченными конфигурацией и baseline.
- У каждой строки baseline и каждого исключения namespace есть причина с условием удаления.
- Ни одна строка baseline не допускает больше находок, чем осталось.
- Сгенерированные файлы `.fos` исключены, а вместо них исправляются генераторы.
- Локальная агрегирующая задача и CI вызывают одну и ту же обёртку проекта.
- `Engine/Source` входит в области проверки привязок всякий раз, когда используется `--scripts`, чтобы нативные события участвовали в сравнении.
