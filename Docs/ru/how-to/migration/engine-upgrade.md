---
layout: default
title: Обновление Engine во встраивающем проекте
locale: ru
document_id: engine-upgrade-guide
permalink: /Docs/ru/how-to/migration/engine-upgrade.html
---

# Обновление Engine во встраивающем проекте
<!-- docs-translation: {"document_id":"engine-upgrade-guide","locale":"ru","source_path":"Docs/en/how-to/migration/engine-upgrade.md","source_sha256":"63ab54950d59825afaa7fad5e7e187a43bd027de98dc18385987836188d26f23"} -->
Это руководство задаёт повторяемую процедуру обновления Engine в игровом
репозитории. Она охватывает интеграцию исходников, сгенерированные контракты,
контент, сохранения, сеть, client runtime и документацию. Кроме того, оно
приводит дословные команды Git, рецепты поиска по истории изменений и влияния
на потребителей, правила разбиения длинных диапазонов на серии и скелет плана,
парный к записи о завершении.

## Проверенные исходные пути

- `AGENTS.md`
- `VERSION`
- `.gitignore`
- `BuildTools/docs_contract_diff.py`
- `BuildTools/docs_engine_version.py`
- `BuildTools/engine_version.py`
- `Docs/contract-change-dispositions.json`
- `Docs/generated`
- `Docs/en/reference/changelog.md`
- `Docs/en/how-to/release/versioning.md`
- `Docs/en/how-to/build/embedding-project.md`
- `Docs/en/contributing/contract-change-management.md`
- `Docs/en/explanation/runtime/client-updater.md`
- `Docs/en/explanation/persistence/index.md`
- `Docs/en/how-to/build/project-configuration.md`
- `Docs/en/how-to/build/generated-content.md`
- `Docs/en/reference/platforms/support-matrix.md`
- `Docs/en/contributing/documentation/index.md`

## Определите обновление

До изменения submodule или vendored Engine checkout запишите:

- ревизию корня игрового проекта;
- старую точную ревизию Engine;
- планируемую новую точную ревизию Engine;
- upstream branch/repository;
- поддерживаемую build/runtime matrix;
- имена safety branch или stash;
- владельцев проверки compatibility, persistence, release и документации.

Обновление Engine не является одним изменением указателя. Полный входящий
диапазон Engine и проектные изменения для его принятия образуют единую единицу
review.

Вместе с точными SHA запишите прежний и новый идентификаторы `VERSION` движка и проверьте [историю изменений](../../reference/changelog.md). [CalVer](../release/versioning.md) задаёт год, релизную линию, изменение master и необязательный патч релиза; совместимость по-прежнему требует описанного ниже аудита исходников и контрактов. Для исторических изменений до начала поддерживаемого журнала нужен полный обзор диапазона ревизий.

## Обновление Engine не задаёт политику закрепления Engine

Эта страница описывает, как безопасно перенести закрепление (pin) Engine. Она
не говорит, когда проекту следует это делать. Решение принадлежит проекту:
оставаться на проверенной ревизии, принимать каждый шаг master или следовать
только патчам релиза одного среза (см.
[Срез, стабилизация и патчи](../release/versioning.md#срез-стабилизация-и-патчи)).
Появление более нового commit Engine в upstream никогда не обязывает проект его
принять.

Пока решение не принято, сохраняйте закрепление воспроизводимым:

- проект закрепляет точный commit Engine, а не имя ветки;
- локальные правки внутри checkout Engine не входят в закрепление, и тогда
  идентификатор сборки несёт маркер `-dirty` (`FO_ENGINE_REVISION` в
  [таблице идентификаторов](../release/versioning.md#идентификаторы-и-потребители));
- переиспользуемое исправление, найденное во время обновления, принадлежит
  репозиторию Engine и приходит через последующее закрепление, а не через
  приватный patch submodule (см.
  [принцип проверки](../build/embedding-project.md#принцип-проверки)).

Как submodule добавляется и закрепляется впервые, описано в руководстве по
[созданию репозитория игры](../project/create-repository.md).

## Запишите идентичности Engine

У проекта, встраивающего Engine как Git submodule, есть три независимые
идентичности Engine. Во время каждого обновления они расходятся, поэтому
записывайте каждую отдельно.

| Идентичность | Значение | Команда |
|---|---|---|
| Gitlink родителя в `HEAD` | Закрепление, записанное последним commit проекта | `git ls-tree HEAD Engine` или `git rev-parse HEAD:Engine` |
| Запись индекса | Закрепление, которое запишет следующий commit проекта | `git ls-files --stage Engine` или `git rev-parse :Engine` |
| Checkout Engine | Commit, который фактически собран и проверен | `git -C Engine rev-parse HEAD` |

Сохраните их в переменных и сравните:

```bash
old=$(git rev-parse HEAD:Engine)
staged=$(git rev-parse :Engine)
checkout=$(git -C Engine rev-parse HEAD)

echo "gitlink at HEAD: $old"
echo "index entry:     $staged"
echo "Engine checkout: $checkout"
git submodule status Engine
git -C Engine show "$old:VERSION"
git -C Engine status --short
```

```powershell
$old = git rev-parse HEAD:Engine
$staged = git rev-parse :Engine
$checkout = git -C Engine rev-parse HEAD

Write-Output "gitlink at HEAD: $old"
Write-Output "index entry:     $staged"
Write-Output "Engine checkout: $checkout"
git submodule status Engine
git -C Engine show "${old}:VERSION"
git -C Engine status --short
```

`git submodule status` начинает строку с пробела, когда checkout совпадает с
записью индекса, с `+`, когда checkout от неё отличается, с `-`, когда
submodule не инициализирован, и с `U` при конфликте слияния.
`git diff --submodule=log -- Engine` перечисляет commits между записью индекса и
checkout, а `git diff --cached --submodule=log -- Engine` перечисляет commits
между `HEAD` и записью индекса.

Используйте gitlink в `HEAD` как старую ревизию обновления, потому что именно
эту ревизию проект закоммитил последней. Checkout или запись индекса, которые
уже отличаются от неё, являются незавершённой работой: разрешите или запишите
её до начала и не берите её за baseline. Если checkout не инициализирован или
отстаёт от записи индекса, `git submodule update --init Engine` возвращает его
к записи индекса. Указывайте путь, чтобы не затронуть другие submodules,
которые может содержать проект.

## Сохраните начальное состояние

1. Получите свежие данные из remotes проекта и Engine.
2. Проверьте локальные изменения в обоих worktrees.
3. Создайте именованную safety branch и при необходимости именованный stash в
   обоих репозиториях.
4. Запишите `git rev-parse HEAD`, Engine gitlink и remote tips.
5. Сохраняйте safety refs, пока обновлённый проект не пройдёт чистую проверку.

Не сбрасывайте, не удаляйте и не перезаписывайте несвязанные локальные
изменения ради видимости чистого обновления.

### Safety refs и stash

```bash
git fetch origin
git -C Engine fetch origin

git status --short
git -C Engine status --short

git branch engine-update-safety
git -C Engine branch engine-update-safety "$old"
git -C Engine branch --remotes --contains "$old"

git stash push --message engine-update-safety
git -C Engine stash push --include-untracked --message engine-update-safety
```

```powershell
git fetch origin
git -C Engine fetch origin

git status --short
git -C Engine status --short

git branch engine-update-safety
git -C Engine branch engine-update-safety $old
git -C Engine branch --remotes --contains $old

git stash push --message engine-update-safety
git -C Engine stash push --include-untracked --message engine-update-safety
```

- Выполняйте две команды `stash` только для репозитория, у которого вывод
  `status` не пуст; иначе Git печатает `No local changes to save`.
- Stash локален для своего репозитория, поэтому stash проекта не содержит
  правок worktree Engine. Сохраняйте оба.
- `git -C Engine branch --remotes --contains "$old"` ничего не печатает, если
  старое закрепление существует только локально. Тогда safety branch остаётся
  единственной ссылкой, сохраняющей достижимость этого commit.
- Перечисляйте сохранённые refs командами
  `git branch --list "engine-update-safety"`, `git stash list` и
  `git -C Engine stash list`. Выбирайте имена, идентифицирующие обновление, и
  сохраняйте их, пока запись о завершении не назовёт условие их удаления.

## Получите, закрепите и добавьте в индекс новую ревизию

1. Определите цель как точный commit репозитория Engine. Опубликованный выпуск
   помечен тегом `v<version>`; разрешите тег в commit, а не записывайте имя
   тега.
2. Проверьте, что commit присутствует и что старое закрепление является его
   предком. Цель, для которой старое закрепление не является предком,
   относится к другой линии истории, а не обновляет текущую.
3. Извлеките цель в submodule в режиме detached и добавьте новый gitlink в
   индекс.

```bash
new='<new-engine-sha>'

git -C Engine rev-parse --verify "$new^{commit}"
git -C Engine merge-base --is-ancestor "$old" "$new" && echo "old pin is an ancestor"
git -C Engine log --oneline "$old..$new"

git -C Engine checkout --detach "$new"
git add Engine
git diff --cached --submodule=log -- Engine
```

```powershell
$new = '<new-engine-sha>'

git -C Engine rev-parse --verify "$new^{commit}"
git -C Engine merge-base --is-ancestor $old $new
if ($LASTEXITCODE -eq 0) { Write-Output 'old pin is an ancestor' }
git -C Engine log --oneline "$old..$new"

git -C Engine checkout --detach $new
git add Engine
git diff --cached --submodule=log -- Engine
```

После добавления в индекс три идентичности из раздела [Запишите идентичности Engine](#запишите-идентичности-engine) должны
указывать на новый commit, за исключением `HEAD`: он хранит старый gitlink,
пока не сделан commit проекта. Коммитьте закрепление вместе с изменениями для
его принятия (конфигурация, скрипты, контент, документация), чтобы ни одна
ревизия проекта не записывала закрепление, которое не собирается.

Чтобы отказаться от попытки до commit, верните checkout и индекс к старому
закреплению:

```bash
git -C Engine checkout --detach "$old"
git restore --staged Engine
```

```powershell
git -C Engine checkout --detach $old
git restore --staged Engine
```

## Проверьте полный диапазон Engine

Изучите каждый commit и изменённый путь между старой и новой привязками:

```bash
git -C Engine log --oneline "$old..$new"
git -C Engine diff --stat "$old..$new"
git -C Engine diff "$old..$new" -- \
  Source BuildTools ThirdParty Resources Docs Examples
```

```powershell
git -C Engine log --oneline "$old..$new"
git -C Engine diff --stat "$old..$new"
git -C Engine diff "$old..$new" -- `
  Source BuildTools ThirdParty Resources Docs Examples
```

Ведите по одной заметке на commit (subject, владеющая область, последствие для
проекта). Отсортировать их помогают два более узких списка:

```bash
git -C Engine log --first-parent --oneline "$old..$new" -- VERSION
git -C Engine diff --stat "$old..$new" -- Docs/generated
```

```powershell
git -C Engine log --first-parent --oneline "$old..$new" -- VERSION
git -C Engine diff --stat "$old..$new" -- Docs/generated
```

Первый перечисляет шаги публикации, каждый из которых является commit первого
родителя, изменившим `VERSION`, а значит, и точки, в которых обновление можно
приостановить. Второй показывает, какие сгенерированные модели контрактов
изменились.

Классифицируйте изменения по владельцу и последствиям:

| Изменение | Обязательная проверка |
|---|---|
| CMake option/stage/application/package | Влияние на project configure, target, CI, package и support matrix |
| Project library helper/core-role graph | Project dependency targets, назначение roles, native bridge, platform gates и runtime package |
| Setting/default/config parser | Влияние на `.fomain`, sub-config, secret, resource pack и запуск |
| Script API/metadata/property | Влияние на compile, bake, network, persistence, migration и gameplay |
| Baker/file format/resource runtime | Authored content, forced rebake, cache/output schema и platform |
| Networking/updater/client runtime | Protocol, gameplay compatibility, host/runtime ABI, package и rollout |
| Database/entity serialization | Save migration, backup/restore, rollback и запрет mixed versions |
| Tool/editor | Authoring workflow, round trip, generated files и screenshots/manual |
| Documentation/example | Владение Engine/project, links, commands, pins и translation freshness |

Поведение встраивающего проекта используйте только как integration evidence.
Нормативными остаются исходники, тесты, интерфейсы и сгенерированные модели
Engine.

## Выполните явные инструкции миграции

Проверьте все датированные изменения Engine между старым/новым идентификаторами от старых к новым. Обнаружьте затронутое использование точными поисками и диагностикой каждой записи, выполните все преобразования и промежуточные шаги данных/перепекания, запишите отрицательные проверки старого использования и положительные незатронутого/мигрированного. Поддержка только актуального API означает ошибку компиляции/перепекания/валидации затронутого использования без миграции; первоначальный смысл незатронутых используемых значений сохраняется. Diff формы не выявляет переинтерпретацию семантики. [Исчерпывающая запись миграции](../release/versioning.md#исчерпывающая-запись-миграции) задаёт контракт приёмки.

### Найдите записи для пропущенного диапазона

Источником миграций служит английская история изменений
(`Docs/en/reference/changelog.md`). Её записи имеют заголовки
`## <version> - <date>`, идут от новых к старым, и в каждой записи есть раздел
`### Migration`; русская история изменений содержит те же версии и даты.
Существуют три формы заголовков:

- `## Current - <date>` - первая датированная запись; она обозначает значение
  `VERSION` той же ревизии. `BuildTools/engine_version.py` разрешает её в это
  значение; более старые записи сохраняют точные идентификаторы.
- `## <version> - <date>` - более старый опубликованный шаг master или патч
  релиза.
- `## Unreleased` - область черновика. Читайте и её, если закреплённый commit
  содержит там текст, но это не датированный шаг.

Собственная запись старой ревизии сохраняется в новой истории изменений под её
точной версией (валидатор публикации отклоняет ревизию, которая удаляет
предшествующую запись). Применять нужно записи новее старой версии, вплоть до
новой версии включительно.

1. Прочитайте оба значения `VERSION`:

   ```bash
   old_version=$(git -C Engine show "$old:VERSION")
   new_version=$(git -C Engine show "$new:VERSION")
   echo "$old_version -> $new_version"
   ```

   ```powershell
   $oldVersion = git -C Engine show "${old}:VERSION"
   $newVersion = git -C Engine show "${new}:VERSION"
   Write-Output "$oldVersion -> $newVersion"
   ```

2. Извлеките новую ревизию и перечислите заголовки с номерами строк:

   ```bash
   grep -n '^## ' Engine/Docs/en/reference/changelog.md
   ```

   ```powershell
   Select-String -Path Engine/Docs/en/reference/changelog.md -Pattern '^## '
   ```

3. Либо выведите входящие записи от старых к новым с помощью тех же helpers,
   которые использует сам инструментарий Engine (`changelog_releases` и
   `read_engine_version` в `BuildTools/engine_version.py`). Сохраните скрипт из
   [листинга ниже](#скрипт-входящих-заметок) вне дерева Engine, например в
   scratch-каталоге задачи как `list_incoming_notes.py`, и запустите его:

   ```bash
   python3 list_incoming_notes.py Engine "$old_version"
   ```

   ```powershell
   py -3 list_incoming_notes.py Engine $oldVersion
   ```

   Подходит любой интерпретатор Python 3; на некоторых хостах `python` всё ещё
   указывает на Python 2. Скрипт печатает по одной строке `<version> <date>` на
   каждую входящую запись и ничего больше. Он читает извлечённую историю
   изменений, поэтому сначала извлеките новую ревизию.

4. Откройте каждую перечисленную запись по порядку. Полностью выполните её
   раздел `### Migration` до перехода к следующей, включая промежуточные шаги
   регенерации, bake и данных, и запишите результат в план.

Старая версия без записи в истории изменений предшествует поддерживаемому
журналу или находится на другой релизной линии. Не угадывайте диапазон:
проверьте полный диапазон commits из предыдущего раздела.

`BuildTools/docs_engine_version.py` - собственный валидатор публикации Engine;
диапазонов он не перечисляет. Из проекта с его помощью всё же можно
подтвердить, что входящая история первого родителя состоит из корректно
оформленных шагов публикации, и подсчитать их. Он читает только Git и истории
изменений переданного ему корня Engine:

```bash
python3 Engine/BuildTools/docs_engine_version.py --root Engine --check \
  --branch master --baseline-git-ref "$old" --target-git-ref "$new" --history
```

```powershell
py -3 Engine/BuildTools/docs_engine_version.py --root Engine --check `
  --branch master --baseline-git-ref $old --target-git-ref $new --history
```

Успешный запуск печатает
`Engine update validation passed: <N> publication step(s)`. При ошибке
называется первый шаг, нарушающий правила версии или истории изменений; это
указывает на диапазон, которому нужен полный аудит исходников. Для релизной
линии передайте `--branch release/<year>.<major>` вместо `master`.

### Скрипт входящих заметок

Этот скрипт печатает по одной строке `<version> <date>` на каждую входящую
запись истории изменений, от старых к новым. Он читает историю изменений и
`VERSION` переданного ему checkout Engine.

```python
import sys
from pathlib import Path

engine_root = Path(sys.argv[1])
old_version = sys.argv[2]

sys.path.insert(0, str(engine_root / 'BuildTools'))
from engine_version import changelog_releases, read_engine_version

new_version = read_engine_version(engine_root)
text = (engine_root / 'Docs/en/reference/changelog.md').read_text(encoding='utf-8')
releases = changelog_releases(new_version, text)
versions = [version for version, _ in releases]
if old_version not in versions:
    sys.exit(f'{old_version} has no changelog entry: review the complete revision range instead')

for version, date in reversed(releases[:versions.index(old_version)]):
    print(version, date)
```

## Найдите в исходниках проекта влияние на потребителей

История изменений и отчёт о контрактах называют символы, settings, options,
команды и формы файлов. Для каждого такого имени решите, использует ли его
проект. Собирайте имена из трёх мест: из каждого применимого раздела
`### Migration` (включая предписанные им шаблоны обнаружения), из отчёта о
контрактах ниже и из списка `git diff --stat` проверенного диапазона.

Запишите по одному имени в строке в текстовый файл, например
`update-names.txt`, и выполните поиск по отслеживаемым исходникам проекта, без
submodule Engine. `git grep` не спускается в submodules, поэтому исключение в
pathspec лишь документирует намерение:

```bash
git grep -nIF -f update-names.txt -- . ':!Engine'
```

```powershell
git grep -nIF -f update-names.txt -- . ':!Engine'
```

Команда одинакова в обеих оболочках. `-F` трактует каждую строку как буквальный
текст, `-I` пропускает бинарные файлы, а код выхода `1` означает отсутствие
совпадений, что и служит отрицательным доказательством для этого списка. Ищите
в каждой из этих областей, потому что одно и то же имя встречается в разных
формах:

- исходники скриптов каждого включённого backend, включая тестовые скрипты;
- исходники native extensions и bridge-код, подключающий заголовки Engine;
- `CMakeLists.txt`, подключаемые файлы `.cmake`, `CMakePresets.json` и
  объявления пакетов;
- главный `.fomain`, каждый sub-config и скрипты CI или запуска, передающие
  settings в командной строке;
- authored content, называющий изменённые setting, property или форму файла;
- документация проекта, цитирующая упразднённое имя.

Ищите во входных данных, а не в сгенерированном или запечённом выводе. Имя,
найденное только в сгенерированном файле, указывает на вход его генератора,
который поиск тоже должен был найти.

Для каждого имени запишите, где оно найдено, какое преобразование применено и
какая положительная проверка доказывает, что мигрированное использование
ведёт себя как прежде. Имени без совпадений нужно отрицательное доказательство,
которого требует запись миграции: отсутствие результатов поиска и, для
динамически собираемых имён, ошибка компиляции, bake или валидации, которую
вызвало бы немигрированное использование. ID записей в отчёте о контрактах
содержат видимое пользователю имя после своего префикса (например, имя option
в ID `cmake.option.`); ищите это имя.

## Сравните сгенерированные контракты

Каждая ревизия Engine коммитит свои сгенерированные модели контрактов в
`Docs/generated/`. Когда новая ревизия извлечена, эти файлы являются текущей
стороной сравнения. Не перегенерируйте их внутри закреплённого submodule:
генераторы являются инструментами сопровождающих Engine и изменили бы
отслеживаемые файлы Engine.

Запустите comparator из корня проекта относительно корня Engine, указав старый
commit Engine как baseline:

```bash
python3 Engine/BuildTools/docs_contract_diff.py \
  --root Engine \
  --baseline-git-ref "$old" \
  --write
```

```powershell
py -3 Engine/BuildTools/docs_contract_diff.py `
  --root Engine `
  --baseline-git-ref $old `
  --write
```

Все пути этого инструмента разрешаются внутри `--root`, а не внутри проекта:

- `--current-dir` по умолчанию равен `Docs/generated`, `--dispositions` по
  умолчанию равен `Docs/contract-change-dispositions.json`, а отчёты по
  умолчанию пишутся в `Workspace/contract-diff.json` и
  `Workspace/contract-diff.md`; с `--root Engine` это `Engine/Docs/generated`,
  `Engine/Docs/contract-change-dispositions.json` и
  `Engine/Workspace/contract-diff.*`.
- Путь, выходящий за пределы корня, отклоняется с `escapes the engine root`,
  поэтому отчёты нельзя записать в проект. `.gitignore` Engine игнорирует
  `/Workspace`, поэтому файлы не загрязняют submodule. После запуска скопируйте
  их в scratch-каталог задачи.
- `--baseline-git-ref` - Git-ревизия репозитория Engine. `--baseline-dir`
  должен быть каталогом внутри корня Engine, содержащим все семнадцать файлов
  моделей.
- `--check` вычисляет результат и печатает только итоговую строку
  `Contract diff <status>: <N> changes across 17 domains, ...`. Чтобы получить
  Markdown-отчёт по каждой записи, используйте `--write`.

Что можно запускать из проекта, а что нельзя:

| Элемент | Из проекта |
|---|---|
| Comparator с `--root Engine`, `--baseline-git-ref`, `--check` или `--write` | Запускается; сообщает изменённые stable entry IDs и их классификацию |
| `Docs/contract-change-dispositions.json` | Ledger, принадлежащий Engine. Читайте его ради обоснования и заметок о совместимости изменения Engine; никогда не добавляйте в него запись проекта |
| Записи missing-disposition в отчёте | Работа репозитория Engine. Проект не может их закрыть; сообщите о пробеле сопровождающим Engine |
| Генераторы моделей (`BuildTools/docs_*.py --write`) и CI gate | Только репозиторий Engine |
| `git -C Engine diff --stat "$old..$new" -- Docs/generated` | Запускается без Python; показывает, какие модели изменились |

`--enforce` - merge gate Engine. Он завершается ошибкой, когда отсутствует
обязательный disposition, а каждая запись ledger привязана к одной точной паре
baseline- и current-digest контракта домена. На многошаговом диапазоне digests
и ID изменений отличаются от digests и ID отдельных шагов, поэтому записи
ledger этих шагов не совпадают, даже если каждый шаг получил disposition. Для
пропущенного диапазона запускайте comparator без `--enforce` и читайте отчёт;
используйте `--enforce` только для диапазона из одного шага публикации.
Baseline старше первой модели домена завершается ошибкой
`does not exist at baseline revision`; тогда `--allow-missing-baseline`
сообщает этот домен как `bootstrap` вместо сравнения, поэтому покройте его
историей изменений и аудитом commits.

Для каждого изменения определите:

- является ли оно additive, documentation-only, policy-only или breaking;
- текущую stability promise;
- затронутые project code/content;
- необходимость migration и release note;
- минимальную совместимую ревизию client/server/save;
- остаётся ли rollback возможным после преобразования данных.

Не считайте метку `internal` доказательством отсутствия влияния на проект. Она
означает лишь, что Engine не дал публичного обещания совместимости. Отчёт также
не видит поведения за неизменёнными декларациями, протокола обновления или
project settings без канонической модели; они опираются на историю изменений и
аудит исходников. Сохраняйте старый и новый отчёты как evidence обновления.

## Разбейте длинный диапазон на серии

Диапазон из многих шагов публикации не обязательно принимать за один раз.
Каждый commit первого родителя, изменяющий `VERSION`, является опубликованным
шагом с собственной датированной записью миграции, поэтому он служит
допустимым промежуточным закреплением; перечислите такие шаги приведённой выше
командой `git log --first-parent --oneline "$old..$new" -- VERSION`. Commits
внутри влитой ветки не являются шагами публикации и не имеют собственной
записи миграции.

1. Прочитайте запись миграции каждого шага диапазона, прежде чем выбирать
   серии.
2. Завершайте серию на каждом шаге, запись которого требует границы, которую
   проект должен пройти и проверить, прежде чем продолжать:
   - преобразование сохраняемых данных или изменение `MigrationRule`, которому
     нужны backup, изолированный restore и решение об откате;
   - принудительный rebake или изменение схемы ресурсов;
   - изменение маркера сетевой совместимости, протокола обновления или ABI
     хоста и runtime-модуля клиента;
   - удалённые или заменённые CMake option, stage, поле пакета, setting или
     script API, требующие правки файлов проекта;
   - ломающее изменение контракта в публичном домене.
3. Внутри одной серии применяйте записи от старых к новым и согласуйте всё один
   раз: один configure, один forced bake, если он нужен хотя бы одной записи,
   один отчёт о контрактах, один проход по документации, один раздел плана.
4. В конце серии выполните проверку на полной заявленной матрице, закоммитьте
   закрепление вместе с изменениями для его принятия и сохраните его safety
   ref. Следующая серия начинается с этого commit как с нового `$old`.
5. Не пересекайте границу миграции внутри серии ради экономии времени.
   Выполнение записей пропущенных обновлений от старых к новым с соблюдением
   промежуточных границ данных и bake является требованием
   [исчерпывающей записи миграции](../release/versioning.md#исчерпывающая-запись-миграции).
6. Если серия не проходит и причина неясна, разделите её пополам: закрепите
   средний шаг публикации, соберите и проверьте проект на нём и продолжайте с
   той половиной, где возникает ошибка.

Шаг, запись которого говорит, что миграция проекта не нужна, всё равно нужно
прочитать; его можно пересечь внутри серии, но нельзя пропустить.

## Согласуйте конфигурацию проекта

Сравните CMake root и `.fomain` проекта с:

- сгенерированным [справочником CMake](../../reference/cmake/index.md);
- [руководством по локальным зависимостям проекта](../../../ProjectDependencies.md);
- сгенерированным [справочником settings](../../../generated/api/settings.md);
- [руководством по конфигурации проекта](../build/project-configuration.md);
- [руководством по безопасности и секретам](../release/security-and-secrets.md);
- изменившимися validation/package interfaces BuildTools.

Удалите устаревшие options и targets, явно добавьте обязательные значения,
проверьте defaults и каждый sub-config для CI, разработки, staging и production.
Повторно проверьте `$ENV`/`$FILE` относительно
`$TARGET_ENV`/`$TARGET_FILE`, command-line masking tokens, side-specific baked
configs, передачу package signing и CI jobs с секретами. Конфиг проекта должен
фиксировать осознанные продуктовые решения, а не случайно наследовать новый
default.

## Пересоберите сгенерированные и запечённые данные

Следуйте [Generated Content Workflow](../build/generated-content.md) в порядке
зависимостей:

1. свежие configure/code generation;
2. native compile;
3. script compile;
4. forced resource bake при изменении contracts или pack inputs;
5. сравнение side-specific metadata;
6. project-generated references и snippet inventory;
7. localization status;
8. site routes, navigation и search;
9. AI evaluation и delivery artifacts.

Сохраняйте старый и новый отчёты о сгенерированных контрактах как evidence
обновления. Не редактируйте generated source или baked output вручную.

## Защитите сохраняемое состояние

Перед проверкой на ценных данных:

1. создайте и проверьте backup;
2. отрепетируйте restore в изолированную database;
3. определите property/prototype/version migration rules;
4. проверьте обновление на представительной копии;
5. проверьте entity counts, ownership, критические fields и пути login/loading;
6. решите, обратима ли migration;
7. запретите старым binaries открывать преобразованные данные, если rollback
   небезопасен.

Правила rename/remove для property и prototype являются runtime-контрактами, а
не удобством очистки. Обновите ссылки в authored content и scripts, сохраняйте
migration rules на поддерживаемый горизонт сохранений и проверяйте
missing/legacy values.

Выполните provider-neutral процедуру из [Backup and Recovery](../release/backup-and-recovery.md).
Абстракция database в Engine не выбирает provider, schedule, retention, schema
rollout, RPO/RTO или полномочия disaster recovery игры; храните эти решения и
evidence в операционной документации проекта.

## Защитите совместимость сети и client

Проверьте три независимые границы:

- gameplay `CompatibilityVersion`;
- поколение updater protocol;
- замороженный client host/runtime ABI.

Не предполагайте, что одна версия покрывает остальные. Разрыв protocol/ABI
может потребовать полного client package и ручной переустановки, даже когда
ресурсы умеют самообновляться. Изменение gameplay compatibility может
запретить смешанные ревизии client/server без изменения updater wire format.

Для online rollout определите:

1. принимаемую когорту старых clients;
2. путь resource/native update;
3. порядок развёртывания server;
4. reconnect behavior;
5. rollback point;
6. пользовательское восстановление для несовместимых frozen hosts;
7. monitoring ошибок update, login, sync и migration.

Точная текущая граница host/runtime и updater описана в
[Client Runtime and Updater](../../explanation/runtime/client-updater.md). Выполняйте
deployment, readiness, graceful stop и rollback по
[Release Operations](../release/operations.md).

## Проверьте принятие обновления

Сначала выполните самые узкие проверки, затем всю заявленную матрицу проекта:

- Engine unit tests для изменённых native domains;
- configure и compile каждым поддерживаемым host compiler;
- `CompileAngelScript` и/или `CompileManagedScripts` для каждого включённого проектом scripting backend;
- `ForceBakeResources` при изменении data graph;
- focused content/gameplay tests;
- starter/tutorial smoke при изменении integration mechanics;
- visible client scene для rendering, GUI, audio, video, input, maps или assets;
- persistence upgrade/restore rehearsal;
- client/server compatibility и updater route;
- package contents и install/launch;
- synthetic-secret checks по baked configs, package trees, archives, logs и
  signing handoff;
- documentation generators, links, locale freshness, site artifact и AI
  delivery.

Соотносите заявления с [Матрицей поддержки](../../reference/platforms/support-matrix.md).
Cross-build не является device qualification, а headless test не доказывает
работу видимого client. Регулярные проверки проекта, выполняемые при каждом
изменении, описаны в [проверках качества](../project/quality-gates.md);
обновление Engine тоже их выполняет, но они не заменяют перечисленных выше
проверок, специфичных для миграции.

## Обновите документацию в той же работе

Согласуйте:

- переиспользуемое поведение в `Engine/Docs/`;
- project integration и product policy в документации игрового проекта;
- routing в `AGENTS.md`, если изменились владение или обязательная процедура;
- публичные примеры и точные Engine pins;
- сгенерированные API/format/settings/package references;
- support matrix и platform guides;
- английские исходные страницы и каждый существующий перевод, чей source hash
  изменился;
- active plan, update record и verification report.

Документация проекта может ссылаться на механики Engine, но не должна их
дублировать. Документы Engine не должны использовать приватный игровой
репозиторий как нормативное доказательство.

## Скелет плана обновления

Создайте план до первого checkout и поддерживайте его актуальным; запись о
завершении ниже является его окончательной формой. Адаптируйте разделы под
проект, но сохраните baseline и target обоих репозиториев, заметки по каждому
commit и ограничения квалификации.

```text
Goal:
  Why the project moves the Engine pin now, and the exit criterion.

Baseline and target:
  Project repository: baseline <sha>, branch <name>, safety ref <name>
  Engine repository:  gitlink at HEAD <sha> (<VERSION>)
                      index entry <sha>
                      checkout <sha>
                      target <sha> (<VERSION>), source <remote/tag>
  Ancestry check (old is an ancestor of target): <result>
  Local changes preserved: <stash names / none>

Incoming range:
  Publication steps (first-parent, VERSION changes): <list>
  Commits audited: <count>
  Per commit:
    <sha> <subject> | owner area | project consequence | action
  Changelog entries applied oldest to newest: <version list>
  Generated contract report: <path>, status, changed entries, domains without a baseline

Project consumer analysis:
  Names searched (from migrations and contract report): <file>
  Per name: matches | transformation | positive check | negative proof

Steps:
  1. <batch or step> - boundary: <data/bake/protocol/CMake/none>
     Acceptance: <observable result>
  2. ...

Verification record:
  Configure and compile per host and target: <commands, results>
  Script compile per backend: <results>
  Forced bake and side metadata comparison: <results>
  Focused tests, smoke, visible-client checks: <results>
  Save upgrade and restore rehearsal: <results>
  Compatibility, updater and package checks: <results>
  Documentation and translation checks: <results>

Qualification limits:
  Platforms, devices and rollout steps not observed: <list, marked pending>
  Owner decisions still open: <list>

Safety refs retained until: <condition>
```

## Запись о завершении

Запись об обновлении должна содержать:

```text
Project old/new:
Engine old/new:
Incoming Engine commits audited:
Generated contract report:
Required dispositions:
Configuration changes:
Content/resource migrations:
Save migration and restore evidence:
Network/updater/ABI decision:
Validated host/target matrix:
Visible/device checks:
Documentation and translation status:
Known residual risks:
Safety refs retained until:
```

Не называйте обновление завершённым, пока отсутствует обязательное evidence.
Записывайте непроверенную платформу или owner-gated deployment как pending, а
не выводите успех из соседних проверок.
