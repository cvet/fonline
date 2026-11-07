---
layout: default
title: Инструменты выпуска для игрового проекта
locale: ru
document_id: project-release-tooling
permalink: /Docs/ru/how-to/project/release-tooling.html
---

# Инструменты выпуска для игрового проекта
<!-- docs-translation: {"document_id":"project-release-tooling","locale":"ru","source_path":"Docs/en/how-to/project/release-tooling.md","source_sha256":"PLACEHOLDER"} -->
Движок поставляет четыре вспомогательных скрипта для работы игрового репозитория над выпусками и обновлениями: `project_version.py` пишет и проверяет монотонный `VERSION` проекта, `release_preflight.py` проверяет выпуск до постановки тега, `artifact_checksums.py` записывает контрольные суммы SHA-256 готовых пакетов, а `engine_upgrade_report.py` готовит запись об обновлении движка. Это скрипты Python 3.12 на стандартной библиотеке; они читают Git и никогда не выполняют fetch, не ставят теги, не делают push и ничего не публикуют. Политика проекта (схема нумерации, границы разрядов, префикс тега, ветка выпуска, имена ассетов) остаётся в параметрах команд и в одном небольшом JSON-файле, которым владеет проект.

## Проверенные исходные пути

- `BuildTools/project_version.py`
- `BuildTools/release_preflight.py`
- `BuildTools/artifact_checksums.py`
- `BuildTools/engine_upgrade_report.py`
- `BuildTools/engine_version.py`
- `BuildTools/package.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/tests/test_project_version.py`
- `BuildTools/tests/test_release_preflight.py`
- `BuildTools/tests/test_artifact_checksums.py`
- `BuildTools/tests/test_engine_upgrade_report.py`
- `Docs/en/how-to/project/versioning-and-releases.md`
- `Docs/en/how-to/project/continuous-integration.md`
- `Docs/en/how-to/migration/engine-upgrade.md`
- `Docs/en/how-to/release/packaging.md`

## Выберите инструмент

| Задача | Команда | Что пишет |
|---|---|---|
| Продвинуть версию проекта перед коммитом или проверить её в CI | `project_version.py` | `VERSION` (только при генерации) |
| Проверить выпуск, разделить большие ассеты, перечислить их хеши | `release_preflight.py` | Части после разделения, манифест и файлы из `--outputs-file` и `--summary-file`, если они запрошены |
| Записать или проверить контрольные суммы готовых пакетов | `artifact_checksums.py` | Sidecar-файлы `<name>.sha256` и необязательный манифест |
| Подготовить запись об обновлении закрепления движка | `engine_upgrade_report.py` | Markdown-запись в стандартный вывод или в `--output` |

Общие правила:

- Запускайте скрипты Python 3.12 или новее: `py -3` на Windows, `python3` на Linux. На некоторых хостах `python` всё ещё Python 2.
- Команды на этой странице выполняются из корня проекта, а движок извлечён в `Engine/`. В отдельном checkout движка уберите префикс `Engine/`.
- Код выхода `0` означает успех, `1` — проваленную проверку или ошибку Git либо файла (сообщение выводится в стандартный поток ошибок), `2` — ошибку использования из разборщика аргументов.
- Точные параметры и тексты справки находятся в сгенерированном [справочнике вспомогательных CLI](../../reference/helper-cli/commands.md).
- Инструментам, которые сравнивают версии, нужна полная история. Неглубокий (shallow) клон отклоняется; в CI выполняйте checkout с полной глубиной и тегами.

## Создайте и проверьте версию проекта

`project_version.py` реализует генератор, которого требует страница [Версионирование и выпуск игрового проекта](versioning-and-releases.md#дайте-игре-сгенерированную-версию) для закоммиченного файла `VERSION`.

Генерация, режим по умолчанию:

1. Если `VERSION` отличается от `HEAD` (в индексе, в рабочем дереве или как неотслеживаемый файл), файл проверяется и остаётся без изменений. Так же в репозиторий попадает первое начальное значение.
2. Иначе скрипт читает `VERSION` в `--base-ref`, берёт наибольшее значение, закоммиченное в истории `HEAD` и базовой ссылки, и записывает следующее за ним значение схемы с окончанием строки LF.

`--check` проверяет файл рабочего дерева и завершается ошибкой, если формат неверен или значение ниже любого значения в истории `HEAD` (и `--base-ref`, если он задан). Равное значение проходит, поэтому коммит, который не продвигает версию, остаётся допустимым.

```bash
python3 Engine/BuildTools/project_version.py --base-ref origin/main
python3 Engine/BuildTools/project_version.py --check
```

```powershell
py -3 Engine/BuildTools/project_version.py --base-ref origin/main
py -3 Engine/BuildTools/project_version.py --check
```

Типичный вывод:

```text
project_version: VERSION: 1.4.316 -> 1.4.317 (base origin/main: 1.4.316)
project_version: VERSION 1.4.317 is valid; reachable floor 1.4.316
```

Файл содержит одну строку, вокруг значения ничего нет; допускается одно окончание строки LF или CRLF. Закоммиченное значение, которое схема отклоняет, проваливает оба режима, а не пропускается. Отсутствующая или нечитаемая базовая ссылка — ошибка, а не тихий пропуск.

### Встроенные схемы и границы

| `--scheme` | Формат | Следующее значение |
|---|---|---|
| `patch-per-commit` (по умолчанию) | `MAJOR.MINOR.PATCH`, неотрицательные целые числа без ведущих нулей | `PATCH + 1` |
| `counter` | Одно неотрицательное целое число | Значение `+ 1` |

`--limit COMPONENT=N` задаёт компоненту исключающую верхнюю границу. Компонент, достигший границы, сбрасывается в ноль и переносится в предыдущий; исчерпанный первый компонент — ошибка. Значения за границей не проходят проверку. Компоненты называются `major`, `minor` и `patch` для `patch-per-commit` и `value` для `counter`. С `--limit patch=10000 --limit minor=10` значение `0.3.9999` переходит в `0.4.0`, а `0.9.9999` — в `1.0.0`. По умолчанию границ нет, поэтому выбирайте их по всем потребителям значения: Windows-пакету `Wix` нужно от двух до четырёх числовых полей в диапазонах Windows Installer, а игре, которая кодирует версию в другое число, нужны границы, сохраняющие это кодирование корректным.

### Храните настройки в файле

`--config <file>` читает JSON-объект с любыми из ключей `file`, `base_ref`, `scheme`, `limits` и `since_commit`; параметры командной строки имеют приоритет. Неизвестные ключи отклоняются. `file` и относительный путь к файлу схемы разрешаются относительно `--root`, который по умолчанию равен текущему каталогу.

```json
{
  "file": "VERSION",
  "base_ref": "origin/main",
  "scheme": "patch-per-commit",
  "limits": {"patch": 10000, "minor": 10}
}
```

Если проект сменил кодирование, `--since-commit <sha>` (или `since_commit`) игнорирует значения, закоммиченные до этого коммита, и старое значение прежнего кодирования больше не задаёт нижнюю границу. Коммит должен быть предком `HEAD` и базовой ссылки.

### Используйте собственную схему проекта

`--scheme <file.py>:<factory>` или `--scheme <package.module>:<factory>` загружает схему проекта. Фабрика получает словарь `limits` и возвращает объект с тремя методами: `parse(text)` возвращает кортеж целых чисел, упорядоченный так же, как версии, и бросает `ValueError` для недопустимого текста, `bump(value)` возвращает следующий кортеж, а `format(value)` возвращает текст файла. Имя модуля импортируется с `--root` в пути импорта.

```python
class ReleaseCounterScheme:
    """r<number> versions that advance by a configurable step."""

    def __init__(self, limits):
        self.step = limits.get('step', 1)

    def parse(self, text):
        if not text.startswith('r') or not text[1:].isdigit():
            raise ValueError('expected r<number>')
        return (int(text[1:]),)

    def bump(self, value):
        return (value[0] + self.step,)

    def format(self, value):
        return f'r{value[0]}'


def create_scheme(limits):
    return ReleaseCounterScheme(limits)
```

Сохраните его как `Tools/version_scheme.py` и передайте `--scheme Tools/version_scheme.py:create_scheme`.

## Выполните предварительную проверку выпуска

`release_preflight.py` заменяет встроенную shell-проверку в задании выпуска. По порядку он:

1. отклоняет неглубокий репозиторий и отслеживаемые файлы, которые отличаются от `HEAD` (неотслеживаемые файлы, например скачанные ассеты, допускаются);
2. читает файл версии и проверяет его теми же параметрами схемы, что и `project_version.py` (`--config`, `--file`, `--scheme`, `--limit`);
3. выводит тег как `--tag-prefix` (по умолчанию `v`) плюс версия и проверяет его через `git check-ref-format`;
4. собирает проблемы: тег уже есть локально (или в `--remote`), версия не превышает новейший существующий тег с этим префиксом, а с `--engine Engine` зафиксированный gitlink движка недостижим ни из одной remote-tracking-ветки submodule;
5. выбирает режим: `publish` только тогда, когда `--ref` (по умолчанию ветка, извлечённая в `HEAD`) равен `--release-ref` и `--dry-run` не задан; любой другой запуск — `dry-run`. Короткая ссылка, например `main`, означает `refs/heads/main`;
6. в режиме `publish` завершается ошибкой при любой найденной проблеме, а в режиме `dry-run` только предупреждает;
7. с `--assets-dir` проверяет ассеты и при необходимости делит и хеширует их.

```bash
python3 Engine/BuildTools/release_preflight.py --ref "$GITHUB_REF" --release-ref refs/heads/main \
  --remote origin --engine Engine \
  --assets-dir dist --max-asset-bytes 1900M --manifest SHA256SUMS
```

```powershell
py -3 Engine/BuildTools/release_preflight.py --ref refs/heads/main --release-ref refs/heads/main `
  --remote origin --engine Engine `
  --assets-dir dist --max-asset-bytes 1900M --manifest SHA256SUMS
```

Стандартный вывод — устойчивый список `key=value`; этот получен запуском с `--max-asset-bytes 2K`:

```text
mode=publish
version=1.4.317
tag=v1.4.317
previous_tag=v1.4.316
asset=948fe603f61dc036b5c596dc09fe3ce3f3d30dc90f024c85f3c82db2ccab679d  6  Game-Client.zip
asset=e5a00aa9991ac8a5ee3109844d84a55583bd20572ad3ffcd42792f3c36b183ad  2048  Game-Server.zip.part00
```

Ассеты и их выходные файлы:

- Ассеты — обычные файлы в корне `--assets-dir`, подходящие под повторяемый glob `--asset-pattern` (по умолчанию каждый файл); sidecar-файлы `.sha256` и сам манифест исключаются. Отсутствие ассетов, пустой ассет, ссылка или небезопасное имя файла — ошибка.
- `--max-asset-bytes SIZE` (с необязательным суффиксом `K`, `M` или `G`, степени 1024) делит каждый ассет большего размера на `<name>.part00`, `<name>.part01` и так далее, не более 100 частей. Части пишутся во временные файлы, их конкатенация сравнивается с дайджестом оригинала, и только затем они переименовываются на место, а оригинал удаляется. Уже существующая часть с тем же именем останавливает запуск. Хостинги вроде GitHub Releases ограничивают размер одного ассета; выбирайте границу ниже этого предела.
- `--manifest SHA256SUMS` пишет один манифест итоговых ассетов (включая части) в формате `sha256sum`.
- `--outputs-file "$GITHUB_OUTPUT"` дописывает строки `version`, `tag`, `previous_tag`, `publish` (`true` или `false`) и `assets`; `--summary-file "$GITHUB_STEP_SUMMARY"` дописывает Markdown-таблицу. Первый вариант подходит любой CI-системе, которая читает файлы `key=value`.

Игроки собирают разделённые части перед распаковкой:

```bash
cat Game-Server.zip.part* > Game-Server.zip
sha256sum --check SHA256SUMS
```

```powershell
cmd /c copy /b Game-Server.zip.part00 + Game-Server.zip.part01 + Game-Server.zip.part02 Game-Server.zip
```

Предварительная проверка не создаёт тег. Ставьте тег на точный коммит после приёмки артефактов, последним шагом перед публикацией, как описано в разделе [Пометьте выпуск тегом](versioning-and-releases.md#пометьте-выпуск-тегом).

## Запишите контрольные суммы пакетов

`artifact_checksums.py` хеширует готовые пакеты после остановки последнего записывающего пакеты процесса и до копирования файлов в место публикации.

```bash
python3 Engine/BuildTools/artifact_checksums.py Workspace/output/Game-Release --manifest SHA256SUMS
python3 Engine/BuildTools/artifact_checksums.py Workspace/output/Game-Release --manifest SHA256SUMS --verify
```

```powershell
py -3 Engine/BuildTools/artifact_checksums.py Workspace/output/Game-Release --manifest SHA256SUMS
py -3 Engine/BuildTools/artifact_checksums.py Workspace/output/Game-Release --manifest SHA256SUMS --verify
```

- По умолчанию выбирается каждый файл корня, имя которого оканчивается на `.zip`, `.tar`, `.tar.gz`, `.msi` или `.apk` без учёта регистра. Повторяйте `--suffix .exe` (и так далее), чтобы заменить этот набор, или повторяйте `--artifact <relative path>`, чтобы явно выбрать вложенные файлы. Распакованные деревья пакетов `Raw` при просмотре по умолчанию не выбираются.
- Каждый sidecar-файл `<name>.sha256` содержит одну строку UTF-8, `<lowercase hex>  <leaf name>`, поэтому `sha256sum --check <name>.sha256` работает из его каталога. Манифест перечисляет каждый выбранный файл по пути относительно каталога в отсортированном порядке. `--no-sidecars` пишет только манифест.
- Каждый входной файл хешируется до подготовки любых выходных файлов; выходные файлы пишутся во временные файлы, которые затем переименовываются на место. Файл, изменившийся во время или после хеширования, абсолютный путь или путь к родителю, ссылка, CR, LF или зарезервированный в Windows символ в имени — ошибка, а сбой оставляет прежние файлы контрольных сумм на месте.
- `--verify` пересчитывает хеши и сравнивает их с существующими sidecar-файлами и манифестом; это работает и на хостах Windows без `sha256sum`. `(Get-FileHash -Algorithm SHA256 <file>).Hash.ToLower()` выводит тот же шестнадцатеричный дайджест.

`BuildTools/package.py` эти файлы не пишет. Один каталог пакета собирается несколькими вызовами (архив `SingleZip` дополняется каждой частью, а `package.py include` добавляет файлы позже), и подпись может следовать за упаковкой, поэтому только задание выпуска знает, когда байты окончательны. Страница [Упаковка и выпуск](../release/packaging.md#воспроизводимость-и-происхождение) требует хешей тех файлов, которые действительно будут опубликованы.

## Подготовьте запись об обновлении движка

`engine_upgrade_report.py` запускается из корня проекта и пишет Markdown-запись, описанную на странице [Обновление Engine во встраивающем проекте](../migration/engine-upgrade.md): факты заполнены, а каждое суждение оставлено как `<fill in>`.

```bash
python3 Engine/BuildTools/engine_upgrade_report.py --names update-names.txt --output Workspace/engine-update/record.md
```

```powershell
py -3 Engine/BuildTools/engine_upgrade_report.py --names update-names.txt --output Workspace/engine-update/record.md
```

- `--old` по умолчанию равен родительскому gitlink в `HEAD`, а `--new` — `HEAD` checkout движка; передайте коммит или тег `v<version>`, чтобы сравнить другие ревизии без их извлечения.
- Запись содержит три идентичности движка и `git submodule status`, проверку предка, каждый коммит диапазона от старых к новым с пустыми столбцами владельца, последствия и действия, шаги публикации по первому родителю, которые меняют `VERSION`, изменённые модели `Docs/generated`, каждую входящую запись истории изменений с процитированным разделом `### Migration` от старых к новым, текст `## Unreleased` цели, однословные фрагменты кода из этих миграций как кандидаты в имена, поиск имён и разделы проверки и завершения.
- `--names <file>` принимает одно буквальное имя на строку и выполняет `git grep -nIF` по проекту вне checkout движка (ограничьте поиск повторяемым `--search-path`). Для каждого имени указывается число совпадений и до `--max-locations` мест; имя без совпадений помечается для отрицательного доказательства.
- История изменений читается на целевой ревизии через `git show`, поэтому checkout может оставаться на месте. Версия базы без записи в этой истории изменений отмечается в записи и предупреждением, потому что тогда нужно просмотреть полный диапазон коммитов.
- Цель, которая не происходит от базы, всё равно даёт запись с пометкой `FAILED` и код выхода `1`.
- Существующий файл `--output` не заменяется без `--force`, поэтому заполненная запись не перезаписывается случайно. Команде нужен инициализированный checkout движка с собственным `.git`.

## Подключите инструменты к задачам и CI

Локальная задача выполняет ту же команду, что и задание CI. Задача VS Code, которая продвигает версию, как один элемент массива `tasks` в `.vscode/tasks.json`:

```json
{
  "label": "Release :: Advance VERSION",
  "type": "process",
  "command": "py",
  "args": ["-3", "Engine/BuildTools/project_version.py", "--config", "Tools/version.json"],
  "options": {"cwd": "${workspaceFolder}"},
  "problemMatcher": []
}
```

На хостах Linux используйте команду `python3` и уберите `-3`. Workflow выпуска затем вызывает инструменты в таком порядке; YAML ниже — паттерн для GitHub Actions, а не готовый файл:

```text
release:
  if: github.event_name == 'workflow_dispatch'
  steps:
    - uses: actions/checkout@v6
      with: {fetch-depth: 0, fetch-tags: true, submodules: true}
    - uses: actions/download-artifact@v7
      with: {name: Game-Release, path: dist}
    - run: python3 Engine/BuildTools/project_version.py --config Tools/version.json --check
    - id: preflight
      run: >-
        python3 Engine/BuildTools/release_preflight.py --config Tools/version.json
        --ref "$GITHUB_REF" --release-ref refs/heads/main --remote origin --engine Engine
        --assets-dir dist --max-asset-bytes 1900M --manifest SHA256SUMS
        --outputs-file "$GITHUB_OUTPUT" --summary-file "$GITHUB_STEP_SUMMARY"
    - if: steps.preflight.outputs.publish == 'true'
      run: <create the tag and the release from steps.preflight.outputs.tag>
```

Запускайте `artifact_checksums.py` в конце задания упаковки, если пакеты публикуются не в выпуске с манифестом, а `project_version.py --check` — в задании проверки каждого изменения. Держите шаг публикации за выходом `publish`; границы заданий и учётных данных описаны в разделе [Запускайте выпуски вручную](continuous-integration.md#запускайте-выпуски-вручную).

## Расширение в проекте

Инструменты движка содержат переиспользуемую механику; проект держит свою политику рядом с ними.

- **Тонкая обёртка.** Положите настройки проекта в `Tools/version.json` и вызывайте инструмент движка из скрипта проекта, чтобы задачи, hooks и CI имели одну точку входа:

  ```python
  import subprocess
  import sys
  from pathlib import Path

  ROOT = Path(__file__).resolve().parents[1]

  # Limits keep the value inside the save-format encoding; see the project versioning page
  command = [sys.executable, str(ROOT / 'Engine/BuildTools/project_version.py'),
             '--root', str(ROOT), '--config', str(ROOT / 'Tools/version.json'), *sys.argv[1:]]
  raise SystemExit(subprocess.run(command).returncode)
  ```

- **Проверки проекта рядом.** Запускайте игровые проверки после инструмента движка в той же задаче: например, что версия по-прежнему преобразуется в версию данных игры, что код версии для магазина сдвинулся вместе с ней или что для нового тега есть заметки о выпуске. Задача должна завершаться ошибкой, если провалился любой шаг.
- **Причины рядом с настройками.** JSON-конфигурация строгая и не имеет поля для комментариев. Записывайте, зачем существует каждая граница, `since_commit` или префикс тега, в комментарии рядом с вызовом обёртки или на странице версионирования проекта и удаляйте настройку вместе с её причиной, когда она перестаёт действовать.
- **Собственная схема.** Если встроенные схемы не выражают политику, поставляйте модуль схемы в проекте и передавайте его через `--scheme`, как показано выше. Держите его тесты в проекте.
- **Переход со скопированного скрипта.** Проект, который держал свой генератор версии, shell-шаг предварительной проверки, скрипт контрольных сумм или фрагмент для перечисления истории изменений, удаляет копию и её тесты, добавляет файл конфигурации и обёртку и один раз запускает `project_version.py --check` на текущей истории. Различия, которые нужно проверить при переходе: отсутствующая базовая ссылка и недопустимое закоммиченное значение теперь приводят к ошибке, а не пропускаются (для старого кодирования используйте `--since-commit`), неотслеживаемый `VERSION` считается изменённым, ведущие нули отклоняются, суффиксы контрольных сумм по умолчанию не включают `.exe` (передайте `--suffix`), а разделение теперь выполняется для каждого ассета больше `--max-asset-bytes` на части не больше этого размера.

## Контрольный список проверки

1. `project_version.py --check` проходит в CI на checkout с полной глубиной, а генерация выполняется перед коммитами, которым нужна новая версия.
2. Границы и `since_commit` заданы по потребителям версии, и их причины записаны.
3. Задание выпуска запускает `release_preflight.py` с `--release-ref`, поэтому любая другая ссылка даёт пробный запуск, и публикует только при `publish=true`.
4. Разделённые ассеты собираются в исходный дайджест, а опубликованный манифест проходит проверку `sha256sum --check` или `artifact_checksums.py --verify`.
5. Контрольные суммы записываются после последнего шага упаковки и подписи, по тем файлам, которые публикуются.
6. Каждое обновление движка начинается с записи `engine_upgrade_report.py`, сохранённой вне checkout движка и завершённой до того, как обновление объявлено законченным.
