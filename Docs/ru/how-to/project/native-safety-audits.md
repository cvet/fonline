---
layout: default
title: Аудит покрытия сущностей и безопасности исключений в игровом проекте
locale: ru
document_id: project-native-safety-audits
permalink: /Docs/ru/how-to/project/native-safety-audits.html
---

# Аудит покрытия сущностей и безопасности исключений в игровом проекте
<!-- docs-translation: {"document_id":"project-native-safety-audits","locale":"ru","source_path":"Docs/en/how-to/project/native-safety-audits.md","source_sha256":"PLACEHOLDER"} -->

`BuildTools/native_safety_audit.py` проверяет нативные исходники C++ на
соответствие двум контрактам движка: нативный код, достижимый из серверного
скриптового экспорта, не должен получать, заменять или освобождать покрытие
сущностей, а каждая классифицированная функция сохраняет проверенный уровень
безопасности исключений с письменной причиной. Движок проверяет этими командами
собственный `Source/` в CI; игровой проект запускает те же команды, добавив
каталоги своих нативных расширений, поэтому к коду проекта применяются те же
правила. На странице описаны команды, форматы конфигурации и baseline, шаблон
для CI и способ расширения аудитов в проекте.

## Проверенные исходные пути

- `AGENTS.md`
- `BuildTools/native_safety_audit.py`
- `BuildTools/native_safety_audit/scope.py`
- `BuildTools/native_safety_audit/sync_scope.py`
- `BuildTools/native_safety_audit/exception_safety.py`
- `BuildTools/native_safety_audit/cpp_parser.py`
- `BuildTools/audit_baselines/sync-scope.tsv`
- `BuildTools/audit_baselines/exception-safety.tsv`
- `BuildTools/tests/test_native_safety_audit_sync_scope.py`
- `BuildTools/tests/test_native_safety_audit_exception_safety.py`
- `BuildTools/HelperCliInterface.json`
- `Source/Server/EntitySync.cpp`
- `Source/Scripting/ServerGlobalScriptMethods.cpp`
- `Docs/en/how-to/scripting/lifecycle-and-concurrency.md`
- `Docs/en/how-to/scripting/managed-csharp.md`
- `Docs/en/contributing/coding-contracts/exception-safety.md`
- `Examples/MinimalMultiplayer/SourceExt/ServerExtension.cpp`
- `.github/workflows/validate.yml`

## Что проверяет каждый аудит

| Команда | Правило движка | Владеющий контракт | Ошибка возникает при |
|---|---|---|---|
| `sync-scope` | Обычные серверные скриптовые вызовы поставляют полное покрытие сущностей; нативные вызываемые функции могут вызывать `ValidateEntityAccess` и `EnsureEntitySynced`, но не должны восполнять недостающее покрытие через `SyncEntity`/`SyncEntities`, `ScopedSyncContext::Sync`, прямое использование `EntityLock`, зондирование покрытия, а также блокировку, сон и повторы при получении покрытия. Сами терминалы удержания покрытия остаются неблокирующими. | `AGENTS.md` (раздел об инвариантах, синхронизации и времени жизни), [нативное удержание покрытых сущностей](../scripting/lifecycle-and-concurrency.md#нативное-удержание-покрытых-сущностей), [серверная синхронизация сущностей](../scripting/managed-csharp.md#серверная-синхронизация-сущностей) | Скриптовом экспорте или зарегистрированном callback `FO_SCRIPT_GENERIC`, граф вызовов которого достигает запрещенного стока |
| `exception-safety` | Уровень безопасности исключений каждой функции (`NoThrow`, `Strong`, `Basic`, `None`) выводится из ее тела и выводится заново при изменении тела. | [Безопасность исключений, раздел 8](../../contributing/coding-contracts/exception-safety.md#8-классификация-безопасности-исключений-по-функциям-уровни-es) | Новой, измененной или удаленной функции без соответствующей строки, строке без причины, неклассифицированной строке, некорректной строке |

Оба аудита написаны на Python 3.12 и используют только стандартную библиотеку.
Они читают только текст исходников: компилятор, `clang`,
`compile_commands.json`, bake и сборка не нужны, и аудиты работают на любом
хосте с Python. Парсер лексический. Он находит функции по форме фигурных скобок
и по соглашениям `FO_TRACE_ZONE` и `FO_SCRIPT_API`, не вычисляет препроцессор и
консервативно разрешает вызовы по имени и типу получателя. Новую функцию без
зоны и без префикса экспорта `exception-safety` не обнаружит, пока ее не назовет
строка baseline, поэтому такие добавления по-прежнему покрывает ревью.

## Запуск аудитов на движке

Из checkout движка в Windows:

```powershell
py -3 BuildTools/native_safety_audit.py sync-scope
py -3 BuildTools/native_safety_audit.py exception-safety
```

В Linux:

```bash
python3 BuildTools/native_safety_audit.py sync-scope
python3 BuildTools/native_safety_audit.py exception-safety
```

Успешный запуск печатает одну итоговую строку на аудит:

```text
sync-scope audit: 4062 functions, 410 script-native roots, 0 violation(s), 0 baselined, 0 error(s)
exception-safety audit: 7701 functions checked, 0 error(s), 0 warning(s)
```

Числа меняются вместе с исходниками. На рабочей станции Windows полный проход
по движку занимает около 12 секунд для `sync-scope` и около 4 секунд для
`exception-safety`.

Находка `sync-scope` называет корень, причину и путь вызовов:

```text
SourceExt/Bad.cpp:3: error: Server_Game_Bad reaches blocking entity-cover replacement: Server_Game_Bad -> SyncContext::SyncEntities
```

С `--json` команда `sync-scope` печатает машиночитаемую сводку, диагностики и
ошибки.

## Коды выхода

| Код | Значение |
|---|---|
| `0` | Находок нет. Предупреждения `exception-safety` (строки `None`, ожидающие решения, заметки для ревью) не приводят к ошибке без `--fail-on warning`. |
| `1` | Находки аудита: нарушения, расхождение классификации, строки без причин, устаревшие или некорректные строки baseline, устаревшие записи конфигурации. |
| `2` | Неверная командная строка или конфигурация: неизвестный параметр, scope вне корня или пересекающийся с `Source/` движка, отсутствующий параметр baseline проекта, некорректный файл конфигурации. |

## Общие параметры

| Параметр | Значение |
|---|---|
| `--root DIR` | Корень для путей `--scope`, `--config` и `--baseline`. По умолчанию текущий каталог. |
| `--engine-root DIR` | Проверяемый checkout движка. По умолчанию checkout, в котором лежит скрипт, поэтому `Engine/BuildTools/native_safety_audit.py` проверяет встроенный движок. |
| `--scope DIR` | Нативный каталог проекта относительно корня, сканируется рекурсивно. Повторяемый. `Source/` движка проверяется всегда и не передается как scope. |
| `--config FILE` | Конфигурация проекта: исключения vendored-кода и проверенные правила проекта. |
| `--baseline FILE` | Baseline проекта для находок в каталогах `--scope`. Для `exception-safety` обязателен, если указан scope. |

Точная справка парсера приведена в [справочнике вспомогательных команд](../../reference/helper-cli/commands.md).

## Сопровождение baseline безопасности исключений

Команда `exception-safety` принимает необязательное действие: `audit` (по
умолчанию), `report` или `update`. Изменение нативного кода проходит такой цикл:

1. Запустите `exception-safety` и прочитайте ошибки.
2. Для новой функции выполните `exception-safety update --add-new`, выведите ее
   уровень по [правилам вывода](../../contributing/coding-contracts/exception-safety.md#8-классификация-безопасности-исключений-по-функциям-уровни-es)
   и впишите в новую строку `level`, `status` (обычно `annotated`) и `reason`.
3. Для измененного тела выведите уровень и причину заново, исправьте строку и
   примите новый хеш тела командой
   `exception-safety update --accept "Source/Client/MapView.cpp::MapView::DrawMap"`.
4. Для изменения сигнатуры без изменения тела выполните
   `exception-safety update --migrate-signatures`; для удаленной функции —
   `exception-safety update --prune-removed`.
5. Запускайте `exception-safety`, пока проверка не пройдет, и фиксируйте
   изменение исходника и изменение baseline вместе.

`report` печатает распределение уровней, строки `None` и очередь ручного ревью;
`report --level Basic --area Server` или `report --file MapView --function DrawMap`
выводят подходящие строки. Уровень описывает факт о текущем теле: никогда не
добавляйте `noexcept` только потому, что строка указывает `NoThrow`.

## Форматы baseline

Каждый baseline — файл UTF-8 с разделителями-табуляциями и фиксированной строкой
заголовка. Файлы движка лежат в `BuildTools/audit_baselines/`; строки движка
называют пути относительно движка (`Source/...`), строки проекта — пути
относительно корня (`SourceExt/...`). Строка движка никогда не покрывает находку
проекта, а строка проекта никогда не покрывает находку движка.

Столбцы `exception-safety`: `file`, `kind`, `function`, `signature`, `body_hash`,
`level`, `status`, `review_note`, `reason`:

```text
file	kind	function	signature	body_hash	level	status	review_note	reason
SourceExt/Run.cpp	marker	Project::Run	void Project::Run()	f3a1c2d4e5b60718	Basic	annotated		Network writes can precede a later throw; the session stays valid
```

| Столбец | Значения |
|---|---|
| `kind` | `marker` (тело начинается с `FO_TRACE_ZONE`), `export` (`FO_SCRIPT_API`), `markerless` (находится по сохраненной сигнатуре), `macro_family` (один `#define`, разворачивающийся в функции) |
| `body_hash` | Хеш тела без комментариев, со схлопнутыми пробелами и удаленными операторами зон; пишет `update`, вручную не заполняется |
| `level` | `NoThrow`, `Strong`, `Basic`, `None`, `Unclassified` |
| `status` | `annotated`, `verified`, `adjudicated`, `unclassified` |
| `review_note` | Необязательная постоянная просьба о ручном ревью; выводится как предупреждение |
| `reason` | Обязательное предложение, объясняющее, почему тело гарантирует свой уровень |

Столбцы `sync-scope`: `file`, `export`, `sink`, `reason`. Строка принимает один
найденный корень и сток до исправления кода; файл движка содержит только
заголовок:

```text
file	export	sink	reason
SourceExt/LegacyDoor.cpp	Server_Door_Open	SyncContext::SyncEntity	Locks the door map while the old door script is being ported
```

Строка, которая больше не совпадает с находкой, считается устаревшей и приводит
к ошибке, поэтому baseline может только сокращаться.

## Файл конфигурации

Конфигурация — строгий JSON: неизвестные ключи, повторяющиеся записи, шаблоны
подстановки в идентичностях и записи без причины являются ошибками (код `2`).
Каждая запись, которая больше ничему не соответствует, проваливает аудит
(код `1`).

```json
{
  "schema_version": 1,
  "exclude": [
    {"path": "SourceExt/ThirdParty", "reason": "Vendored compression library, audited upstream"},
    {"path": "SourceExt/Test_*.cpp", "reason": "Native unit tests are not production code"}
  ],
  "sync_scope": {
    "independent_job_schedulers": [
      {
        "file": "SourceExt/WorkerPool.cpp",
        "function": "Server_Game_StartWorkerPool",
        "reason": "Each pool worker starts as a fresh top-level job that acquires its own cover"
      }
    ],
    "typed_factories": [
      {"name": "GetPoolServer", "type": "ServerEngine", "reason": "Returns the server that owns a worker session"}
    ],
    "deferred_callback_calls": [
      {"name": "Post", "reason": "Queues a callback on the worker pool instead of running it now"}
    ]
  }
}
```

| Ключ | Действие |
|---|---|
| `exclude` | Каталог, файл или шаблон `fnmatch` относительно корня, исключаемый из scope проекта, для vendored-кода. Действует на оба аудита. |
| `independent_job_schedulers` | Точная функция проекта, callbacks которой, переданные в отложенный вызов, стартуют в новом задании верхнего уровня. Из графа вызовов вызывающего скрываются только лямбды, переданные в отложенный вызов внутри этой функции. |
| `typed_factories` | Вспомогательная функция проекта, тип результата которой парсер должен принять для `auto x = Helper(...)`, чтобы вызовы на `x` оставались видимыми в графе вызовов. |
| `deferred_callback_calls` | Дополнительные имена вызовов, кроме `add_job`, `run_thread`, `Submit` и `SetDataArrivedCallback` движка, которые откладывают лямбду внутри проверенного планировщика. |

Правила движка (точные терминалы движка, освобожденные экспорты семейства
`Game.Sync`, доверенные вызывающие регистрации, планировщики движка) встроены и
конфигурацией проекта не меняются.

## Подключение аудитов к локальной задаче и CI

Запускайте оба аудита после форматирования и до bake, рядом с другими
статическими проверками, в порядке из
[поддержания здоровья игрового проекта](quality-gates.md#порядок-агрегирующего-маршрута).
В CI достаточно одного шага Python на аудит; настройка toolchain не нужна:

```bash
python3 Engine/BuildTools/native_safety_audit.py sync-scope --scope SourceExt --config Validation/NativeSafety/config.json --baseline Validation/NativeSafety/sync-scope.tsv
python3 Engine/BuildTools/native_safety_audit.py exception-safety --scope SourceExt --config Validation/NativeSafety/config.json --baseline Validation/NativeSafety/exception-safety.tsv
```

Решение принимается по коду выхода; шаг сам печатает сводку и находки. Файл
baseline, переданный `sync-scope` через `--baseline`, должен существовать, даже
если содержит только заголовок. Никогда не вносите находку в baseline ради
зеленого запуска: исправьте нативный код или запишите проверенную строку с
причиной и удалите ее вместе с исправлением.

## Расширение в проекте

1. Держите файлы проекта вместе, например `Validation/NativeSafety/config.json`,
   `sync-scope.tsv` и `exception-safety.tsv`, под теми же ревьюерами, что и код
   нативных расширений.
2. Добавьте тонкую обертку проекта, которая фиксирует аргументы, чтобы локальная
   задача, hook и CI вызывали одну команду:

   ```python
   import subprocess
   import sys
   from pathlib import Path

   ROOT = Path(__file__).resolve().parents[2]
   AUDIT = ROOT / "Engine" / "BuildTools" / "native_safety_audit.py"
   PROJECT = ["--root", str(ROOT), "--scope", "SourceExt", "--config", "Validation/NativeSafety/config.json"]

   def main() -> int:
       sync = subprocess.call([sys.executable, str(AUDIT), "sync-scope", *PROJECT, "--baseline", "Validation/NativeSafety/sync-scope.tsv"])
       exceptions = subprocess.call([sys.executable, str(AUDIT), "exception-safety", *PROJECT, "--baseline", "Validation/NativeSafety/exception-safety.tsv"])
       return max(sync, exceptions)

   if __name__ == "__main__":
       raise SystemExit(main())
   ```

3. Создайте baseline безопасности исключений проекта командой
   `exception-safety update --add-new` с теми же аргументами; команда запишет
   отсутствующий файл. Перед фиксацией классифицируйте каждую новую строку и
   впишите ее причину.
4. Помещайте проверенные вспомогательные элементы только проекта (планировщики
   рабочих потоков, типизированные фабрики, отложенные очереди) в конфигурацию с
   причиной для каждого, а не в копию правил движка. Правило, которое больше не
   называет функцию, приводит к ошибке, поэтому удаляется вместе с ней.
5. Проверки проекта, которые не являются контрактами движка, держите в отдельных
   скриптах проекта рядом с оберткой и вызывайте из той же задачи. Инструмент
   движка остается без изменений.
6. Храните причины рядом со строками, которые они обосновывают: у каждого
   исключения, правила конфигурации и строки baseline свое поле `reason`, и
   пустая причина приводит к ошибке.

Проект, который вел собственную более раннюю копию этих аудитов, переходит на
инструмент движка одним изменением:

1. Удалите копии скриптов проверки границы синхронизации и безопасности
   исключений вместе с их тестами и направьте локальную задачу и CI на
   `Engine/BuildTools/native_safety_audit.py`.
2. Удалите строки движка (`Source/...`) из baseline безопасности исключений
   проекта; ими владеет baseline движка.
3. Переставьте оставшиеся строки проекта в описанный выше порядок столбцов
   (`review_note` перед `reason`), впишите причину в каждую строку без нее и
   передайте файл через `--baseline`.
4. Перенесите имена вспомогательных функций проекта и идентичности
   планировщиков проекта, которые в старом скрипте были константами, в файл
   конфигурации, а исключения vendored-каталогов — в `exclude`.
5. Запустите оба аудита. Исправьте или объясните каждую новую находку: рекурсивный
   scope проекта может включить файлы, которые прежний нерекурсивный проход
   пропускал.

## Контрольный список проверки

- `sync-scope` и `exception-safety` завершаются с кодом `0` локально и в CI с
  аргументами проекта.
- У каждого исключения, правила конфигурации и строки baseline есть письменная
  причина, и ни одно из них не устарело.
- Baseline безопасности исключений проекта содержит только строки проекта;
  строки движка берутся из checkout движка.
- Каждое нативное изменение обновляет свои строки baseline в том же коммите, с
  заново выведенными уровнями и причинами.
- Ни одна строка baseline `sync-scope` не переживает исправление, которого она
  ожидала.
