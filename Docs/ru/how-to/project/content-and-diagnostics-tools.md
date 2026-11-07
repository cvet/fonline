---
layout: default
title: Инструменты текстовых пакетов, синхронизации движения и управляемого проекта
locale: ru
document_id: project-content-and-diagnostics-tools
permalink: /Docs/ru/how-to/project/content-and-diagnostics-tools.html
---

# Инструменты текстовых пакетов, синхронизации движения и управляемого проекта
<!-- docs-translation: {"document_id":"project-content-and-diagnostics-tools","locale":"ru","source_path":"Docs/en/how-to/project/content-and-diagnostics-tools.md","source_sha256":"PLACEHOLDER"} -->
Это руководство описывает три небольших инструмента движка, которые игровой проект запускает из своих задач и CI: `check_text_packs.py` проверяет исходные текстовые пакеты `.fotxt` до бейкинга, `move_sync_analysis.py` превращает строки журнала `Network.MoveSyncTrace` в отчёты о синхронизации движения по сценариям, а `buildtools.py managed-project-path` выводит, куда бейкер Managed C# записывает сгенерированный проект и решение. Для каждого инструмента даны команды, коды выхода и настройка, а затем показано, как проект добавляет свои правила поверх инструмента, не копируя его.

Все три инструмента читают только форматы, определённые движком: грамматику текстовых пакетов, строки трейса `MOVESYNC` и настройки `ManagedScript`. Выбор языков, обвязка сценариев, пороги приёмки и автоматизация IDE остаются в проекте.

## Проверенные исходные пути

- `BuildTools/check_text_packs.py`
- `BuildTools/move_sync_analysis.py`
- `BuildTools/buildtools.py`
- `BuildTools/foconfig.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/tests/test_check_text_packs.py`
- `BuildTools/tests/test_move_sync_analysis.py`
- `BuildTools/tests/test_buildtools_managed_project.py`
- `BuildTools/tests/fixtures/move_sync`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/codegen.py`
- `Source/Common/TextPack.cpp`
- `Source/Tools/TextBaker.cpp`
- `Source/Common/Movement.h`
- `Source/Common/Movement.cpp`
- `Source/Common/Geometry.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/Settings.cpp`
- `Source/Essentials/Logging.cpp`
- `Source/Client/Client.cpp`
- `Source/Client/CritterHexView.cpp`
- `Source/Server/Server.cpp`
- `Source/Server/Player.cpp`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Tools/ManagedScriptBaker.cpp`
- `Docs/en/how-to/content/text-and-localization.md`
- `Docs/en/explanation/authority-and-networking/index.md`
- `Docs/en/how-to/scripting/managed-csharp.md`
- `Docs/en/how-to/build/project-configuration.md`

## Выбор инструмента

| Инструмент | Что читает | Когда запускать |
| --- | --- | --- |
| `Engine/BuildTools/check_text_packs.py` | Исходные файлы `.fotxt` и `Baking.BakeLanguages` | При изменении текстов или переводов; перед каждым бейкингом в CI |
| `Engine/BuildTools/move_sync_analysis.py` | Журналы сервера и клиентов со строками `MOVESYNC` | После плейтеста или записи с включённым `Network.MoveSyncTrace` |
| `Engine/BuildTools/buildtools.py managed-project-path` | Основной `.fomain` и необязательную подконфигурацию | Чтобы открыть сгенерированное решение C# или передать его путь другому инструменту |

Команды ниже запускаются из корня репозитория, который встраивает движок в каталог `Engine/`. В отдельном checkout движка уберите префикс `Engine/`. Точные usage и справка приведены в [справочнике вспомогательных команд](../../reference/helper-cli/commands.md) и [справочнике команд BuildTools](../../reference/buildtools/commands.md). Команды для Linux проверены через POSIX-оболочку в Windows; macOS не проверялась.

## Проверка текстовых пакетов до бейкинга

Проверка разбирает каждый файл `<TextPack>.<Language>.fotxt` под каждым корнем точно так же, как `TextPack::LoadFromString`, и моделирует нормализацию языков `TextPack::FixPacks` для упорядоченного списка `Baking.BakeLanguages` (см. [нормализацию языков](../content/text-and-localization.md#нормализация-языков)). Каждый корень — один вход пакета ресурсов; пакеты из разных корней не сравниваются.

| Правило | Значение | Что сделал бы бейкинг |
| --- | --- | --- |
| `invalid-file-name` | Имя не имеет вида `<TextPack>.<Language>.fotxt` | Завершится ошибкой |
| `invalid-utf8` | Файл не в UTF-8 | Сохранит неразобранные байты |
| `parse-error` | Нет второго поля, нет закрытого третьего поля или пустой Key1 | Завершится ошибкой `Invalid text file` |
| `unknown-language` | Язык не объявлен | Выдаст предупреждение и пропустит файл |
| `duplicate-pack-file` | Второй файл одного пакета и языка в одном корне | Оставит один файл и проигнорирует другой |
| `duplicate-variant` | Один и тот же текст дважды под одним полным ключом | Оставит оба; один из них никогда не отличим |
| `wrong-script` | Буквы письменности, которую `--script` не разрешает для языка | Отгрузит текст |
| `orphan-pack` | Пакет, которого нет у резервного языка | Удалит пакет |
| `extra-key` | Ключ, которого нет у резервного языка | Удалит ключ |
| `missing-pack` | Пакет, которого нет у языка (отключается `--sparse`) | Скопирует пакет резервного языка |
| `missing-key` | Ключ, которого нет у языка (отключается `--sparse`) | Скопирует текст резервного языка |
| `variant-count-mismatch` | Другое число вариантов, чем у резервного языка (только с `--strict-variant-counts`) | Сохранит оба числа |

Письменность буквы — это первое слово её имени в Unicode в нижнем регистре (`latin`, `cyrillic`, `greek`, `arabic`, `hangul`, `katakana` и так далее, `han` для иероглифов CJK); цифры, пунктуация и пробелы не проверяются никогда. Встроенные цветовые теги движка удаляются до проверки. Подстановки проекта — синтаксис проекта, поэтому удаляйте их через `--ignore-pattern`. Используйте `--sparse` для языка-надстройки, который намеренно наследует большую часть текста от родителя: он может не содержать пакеты и ключи, но по-прежнему не может добавлять ключи, которых нет у родителя.

Windows (PowerShell):

```powershell
py -3 Engine\BuildTools\check_text_packs.py Texts --config MyGame.fomain `
    --script engl=latin --script russ=cyrillic `
    --ignore-pattern '@[A-Za-z_]+@' `
    --baseline Tools\TextPacks\text-pack-baseline.json
```

Linux (bash):

```bash
python3 Engine/BuildTools/check_text_packs.py Texts --config MyGame.fomain \
    --script engl=latin --script russ=cyrillic \
    --ignore-pattern '@[A-Za-z_]+@' \
    --baseline Tools/TextPacks/text-pack-baseline.json
```

`--config` читает `Baking.BakeLanguages` из основной секции, а вместе с `--sub-config` — из этой подконфигурации и её родителей; `--languages 'engl russ'` передаёт объявления напрямую. Каждая находка — одна строка `path:line: rule: key detail`, где строка `0` обозначает находку, относящуюся к файлу или отсутствующему ключу, а не к строке. `--json <file>` дополнительно записывает оставшиеся находки для отчёта или для черновика первого baseline.

| Код выхода | Значение |
| --- | --- |
| `0` | После исключений не осталось ни одной находки |
| `1` | Осталась хотя бы одна находка или устаревшее исключение |
| `2` | Ошибка использования: нечитаемая конфигурация или baseline, неверные объявления, неизвестный язык в `--script` или `--sparse`, неверный шаблон, отсутствующий корень |

### Формат baseline

Baseline принимает известные находки, пока проект разбирает накопленный долг. Каждое исключение указывает правило, путь относительно `--base-dir` (по умолчанию текущий каталог), полный ключ (`{Key1}{Key2}`, пустой для находок уровня файла) и причину. Исключение, которое ни с чем не совпало, сообщается как `stale-exemption`, поэтому файл может только сокращаться. Можно передать несколько файлов `--baseline`.

```json
{
  "schema_version": 1,
  "exemptions": [
    {
      "rule": "missing-key",
      "path": "Texts/Game.russ.fotxt",
      "key": "{Intro}{}",
      "reason": "Intro text is translated together with the rest of the opening chapter."
    }
  ]
}
```

Неизвестные поля, неизвестные правила, пустые причины и повторяющиеся исключения — ошибки использования. У движка нет контента `.fotxt`, и он не поставляет baseline.

## Анализ трейсов синхронизации движения

При включённом `Network.MoveSyncTrace` сервер и каждый клиент пишут по одной строке `MOVESYNC` на каждое событие синхронизации движения; список событий и полей приведён в разделе [Трассировка синхронизации движения](../../explanation/authority-and-networking/index.md#трассировка-синхронизации-движения). Анализатор сводит несколько журналов, режет их на сценарии и сравнивает, где криттера держит сервер, где его рисует собственный клиент и где его видит каждый другой клиент.

### Запись трейса

Включайте настройку только для диагностических прогонов, например в подконфигурации, выбранной через `-ApplySubConfig MoveSyncTrace` (см. [порядок приоритетов](../build/project-configuration.md#порядок-приоритетов)), или переопределением командной строки `-Network.MoveSyncTrace True`:

```ini
[SubConfig]
Name = MoveSyncTrace
Network.MoveSyncTrace = True
```

Сценарии ограничиваются строками, которые проект пишет сам из скрипта или тестовой обвязки через свой вызов журналирования, в том же формате, что и строки движка:

```text
MOVESYNC side=cl ev=mark t=<monotonic microseconds> st=<synchronized milliseconds> viewer=<chosen critter id> label=begin:<name>
MOVESYNC side=cl ev=mark t=<monotonic microseconds> st=<synchronized milliseconds> viewer=<chosen critter id> label=end:<name>
```

`viewer` начальной метки — действующий криттер. `t` должно браться из тех же монотонных часов, что и строки движка; время кадра скрипта — это показание тех же часов в начале кадра за вычетом смещения паузы или `Timer.DeltaTimeCap`. Метки не могут содержать пробелы. Необязательная строка `ev=input` с `viewer`, `state` и `pattern_ms` записывает изменение скриптового ввода и даёт задержку от ввода до отправки. Без меток `--actor <critter id>` разбирает весь трейс как один сценарий.

### Запуск анализатора

Windows (PowerShell):

```powershell
py -3 Engine\BuildTools\move_sync_analysis.py (Get-ChildItem Logs\MoveSync\*.log) `
    --geometry hexagonal --timeline blocked `
    --json Logs\MoveSync\report.json --markdown Logs\MoveSync\report.md
```

Linux (bash):

```bash
python3 Engine/BuildTools/move_sync_analysis.py Logs/MoveSync/*.log \
    --geometry hexagonal --timeline blocked \
    --json Logs/MoveSync/report.json --markdown Logs/MoveSync/report.md
```

`--geometry` обязателен и должен совпадать с `FO_GEOMETRY` проекта (`hexagonal` или `square`), потому что все расстояния считаются по метрике сетки движка. Журналы с одной машины используют общие часы по умолчанию `--clock t`; для журналов с нескольких машин используйте `--clock st`, межпроцессные задержки по нему — оценки. `--settle-ms` продлевает каждое окно после конечной метки, `--timeline <label>` или `--timeline all` выводит сводную шкалу событий, а `--require-converged` превращает расхождение в конце любого сценария в код выхода `1`.

Таблица Markdown кратко описывает каждый сценарий; подробности содержит отчёт JSON. Самые полезные ключи сценария: `converged` (сервер, актёр и каждый наблюдатель, который ещё видит актёра, заканчивают на одном гексе), `settledAtMs`, `server.behindActorMs` и `observerViews.<id>.behindServerMs` (отставание по гексам), `actor.snaps` и `correctionPx` (видимые коррекции), `divergence` (гексо-секунды и секунды расхождения), `frameStallMaxMs` и `server.stallMaxMs` (голодание хоста, а не протокол), `server.finishOutcomes` и `server.uplinkMs`.

| Код выхода | Значение |
| --- | --- |
| `0` | Отчёт записан; с `--require-converged` все сценарии сошлись |
| `1` | Задан `--require-converged`, и хотя бы один сценарий разошёлся |
| `2` | Журнал нечитаем, не найдено ни одной строки `MOVESYNC`, не найдено ни одного сценария или аргумент неверен |

`BuildTools/tests/fixtures/move_sync` содержит журнал сервера и два журнала клиентов, записанные в точном порядке полей эмиттеров движка; на них удобно попробовать команду.

## Поиск сгенерированного управляемого проекта

`ManagedScriptBaker` записывает `<ProjectName>.gen.csproj`, `<ProjectName>.gen.sln` и `FOnline.ManagedHost.gen.csproj` в `ManagedScript.GeneratedDir`. Относительный каталог отсчитывается от каталога основной конфигурации; пустое значение выбирает `<build directory>/GeneratedSource/Managed` дерева сборки CMake (см. [generated project и assemblies](../scripting/managed-csharp.md#generated-project-и-assemblies)). Команда читает те же настройки из основной секции, а вместе с `--sub-config` — из этой подконфигурации и её родителей. `CompileManagedScripts` и `BakeResources` по умолчанию не применяют подконфигурацию, поэтому передавайте `--sub-config` только для цели бейкинга, которая её выбирает.

Windows (PowerShell):

```powershell
py -3 Engine\BuildTools\buildtools.py managed-project-path MyGame.fomain --build-dir Build\windows
py -3 Engine\BuildTools\buildtools.py managed-project-path MyGame.fomain --build-dir Build\windows --open
```

Linux (bash):

```bash
python3 Engine/BuildTools/buildtools.py managed-project-path MyGame.fomain --build-dir Build/linux
python3 Engine/BuildTools/buildtools.py managed-project-path MyGame.fomain --build-dir Build/linux --json
```

Вывод по умолчанию состоит из четырёх строк `generated-dir:`, `project:`, `solution:` и `host-project:`, за каждой следует абсолютный путь. `--json` выводит те же пути, а также `projectExists` и `solutionExists`. `--build-dir` нужен только тогда, когда `ManagedScript.GeneratedDir` пуст. `--require-existing` завершается ошибкой, пока бейкер ещё не запускался. `--open` — единственный параметр, который что-либо запускает: он открывает существующее решение через файловую ассоциацию рабочего стола (`os.startfile` в Windows, `open` в macOS, `xdg-open` в остальных системах). Значения с `+=` или подстановкой `$ENV{}`/`$FILE{}` отклоняются, а не угадываются.

| Код выхода | Значение |
| --- | --- |
| `0` | Пути выведены; с `--require-existing` или `--open` проект и решение существуют и программа открытия запущена |
| `1` | `--require-existing` или `--open` не нашли проект или решение, либо программа открытия завершилась ошибкой |
| `2` | Конфигурация нечитаема, подконфигурация или её родитель неизвестны, `ManagedScript.ProjectName` пуст, `ManagedScript.GeneratedDir` пуст без `--build-dir`, значение не поддерживается или аргумент неверен |

## Подключение инструментов к задачам и CI

Добавьте по одной задаче на инструмент рядом с остальными задачами проекта, выбирая интерпретатор по ОС, как в руководстве по [рабочему пространству VS Code](vscode-workspace.md):

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Check :: Text Packs",
      "type": "shell",
      "windows": { "command": "py", "args": ["-3", "Tools/TextPacks/check_texts.py"] },
      "linux": { "command": "python3", "args": ["Tools/TextPacks/check_texts.py"] },
      "problemMatcher": []
    },
    {
      "label": "Tools :: Open Managed Solution",
      "type": "shell",
      "windows": { "command": "py", "args": ["-3", "Engine/BuildTools/buildtools.py", "managed-project-path", "MyGame.fomain", "--build-dir", "Build/windows", "--open"] },
      "linux": { "command": "python3", "args": ["Engine/BuildTools/buildtools.py", "managed-project-path", "MyGame.fomain", "--build-dir", "Build/linux", "--open"] },
      "problemMatcher": []
    }
  ]
}
```

В CI запускайте проверку текстовых пакетов до задания бейкинга в быстрой полосе, поскольку ей не нужна сборка, и сохраняйте её JSON как артефакт. Анализатор движения запускайте в том задании, которое создаёт трейс, после завершения процессов, а `--require-converged` используйте только тогда, когда сходимость как гейт принадлежит обвязке сценариев. Считайте код выхода `2` сломанным шагом, а не находками в контенте. Полосы и артефакты описаны в руководстве по [непрерывной интеграции](continuous-integration.md).

```bash
python3 Engine/BuildTools/check_text_packs.py Texts --config MyGame.fomain \
    --script engl=latin --baseline Tools/TextPacks/text-pack-baseline.json \
    --json "$ARTIFACTS_DIR/text-packs.json"
```

## Расширение в проекте

Держите политику проекта в тонкой обёртке, которой владеет проект: она вызывает инструмент движка с областью проверки и файлом исключений проекта, а затем рядом выполняет правила, существующие только в проекте. Не редактируйте и не форкайте скрипты движка.

```python
"""Project text-pack gate: the Engine lint with this project's scope, then project-only rules."""

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ENGINE_LINT = ROOT / "Engine" / "BuildTools" / "check_text_packs.py"


def check_project_rules(texts: Path) -> int:
    # Project-only rules, such as key naming or texts that must stay distinct, live beside the Engine call
    return 0


def main() -> int:
    engine = subprocess.run([
        sys.executable, str(ENGINE_LINT), "Texts",
        "--config", "MyGame.fomain",
        "--script", "engl=latin",
        "--script", "russ=cyrillic",
        "--ignore-pattern", "@[A-Za-z_]+@",
        "--baseline", "Tools/TextPacks/text-pack-baseline.json",
    ], cwd=ROOT, check=False)
    return engine.returncode or check_project_rules(ROOT / "Texts")


if __name__ == "__main__":
    raise SystemExit(main())
```

- **Текстовые пакеты.** Языковую политику (равенство числа вариантов для выбранных пакетов, ключи, которые не должны делить один текст, надстройки изданий) размещайте в `check_project_rules` и читайте `.fotxt` собственным читателем проекта или функцией `parse_text_pack` модуля движка. Храните baseline рядом с обёрткой, по одной причине на исключение, и удаляйте исключение в том же изменении, которое исправляет находку; правило устаревших исключений это обеспечивает.
- **Трейсы движения.** Импортируйте модуль и передайте `extensions` в `analyze`: каждое расширение получает окно сценария и его отчёт и возвращает дополнительные данные в `extensions.<name>`. События окна начинаются за две минуты до начальной метки, чтобы позиции, известные раньше, переносились в сценарий; фильтруйте по `window.scenario.begin` и `end`, когда метрика должна покрывать только сам сценарий. Строки стороны, которую пишет проект, например реле, ухудшающего сеть, читаются через `--extra-side` или передачей этой стороны в `parse_logs`. Пороги прохождения (минимальная частота кадров, максимальное отставание для профиля сети) принадлежат обёртке.

```python
"""Project movement report: the Engine analysis plus a metric over the project's relay lines."""

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Engine" / "BuildTools"))

import move_sync_analysis as msa  # noqa: E402


def relay_drops(window: msa.ScenarioWindow, report: dict) -> int:
    begin = window.scenario.begin
    end = window.scenario.end + window.settle_us
    return sum(event.int_field("drop") for event in window.events
               if event.side == "relay" and begin <= event.time(window.clock) <= end)


def main(logs: list[str]) -> int:
    events = msa.parse_logs([Path(log) for log in logs], (*msa.ENGINE_SIDES, "relay"))
    report = msa.analyze(events, geometry="hexagonal", extensions={"relayDrops": relay_drops})
    print(json.dumps(report["summary"], indent=2))
    return 0 if report["summary"]["converged"] == report["summary"]["scenarios"] else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
```

- **Управляемый проект.** Поведение, специфичное для IDE, например повторное использование уже запущенного экземпляра IDE или переход к файлу и строке, остаётся в скрипте проекта, который читает `managed-project-path --json` и работает с `solution` или `project`.

Чтобы перейти с собственной более ранней копии проекта:

1. Замените каждый вызов путём к инструменту движка и добавьте ставшие явными входы: `--geometry` для анализатора, `--script` и `--sparse` для проверки текстов, `--build-dir` для поиска проекта.
2. Перенесите правила и метрики, существующие только в проекте, в обёртку или расширение. Ключи JSON анализатора не изменились, кроме того что id криттера сценария называется `actorId`, а метрики реле проекта больше не встроены.
3. Переведите старые списки исключений в формат baseline, записав причину для каждой строки, и запускайте гейт, пока он не пройдёт без устаревших исключений.
4. Удалите старую копию и её тесты, когда обёртка выполняется в локальной агрегатной задаче и в CI.

## Чек-лист проверки

- Гейт текстовых пакетов запускается из чистого checkout, где есть только Python 3.12, и завершается с кодом `0`; внесённый повторяющийся вариант или лишний ключ даёт код `1`.
- У каждого исключения baseline есть причина, и ни одно не сообщается как устаревшее.
- `--script` покрывает каждый отгружаемый язык, а `--sparse` называет только языки-надстройки.
- Анализатор движения получает реальную геометрию проекта, и его отчёт перечисляет каждый сценарий, отмеченный обвязкой.
- `managed-project-path --require-existing` завершается с кодом `0` после `CompileManagedScripts`, а `--open` открывает решение, только что записанное бейкером.
- Ни один скрипт проекта не копирует и не редактирует файлы в `Engine/BuildTools`.
