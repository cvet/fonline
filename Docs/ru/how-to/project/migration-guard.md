---
layout: default
title: Защита миграций идентичностей
locale: ru
document_id: project-migration-guard
permalink: /Docs/ru/how-to/project/migration-guard.html
---

# Защита миграций идентичностей
<!-- docs-translation: {"document_id":"project-migration-guard","locale":"ru","source_path":"Docs/en/how-to/project/migration-guard.md","source_sha256":"PLACEHOLDER"} -->
Сохранённые сущности и авторский контент ссылаются на прототипы, имена свойств и
значения enum по имени. Переименование или удаление без `///@ MigrationRule`
оставляет сохранённые данные указывающими в пустоту, а повторное объявление
выведенного из употребления имени придаёт старым данным смысл, с которым они
никогда не записывались. `BuildTools/migration_guard.py` ловит оба случая на
ревью: он сравнивает идентичности, которые объявляет проект, с закоммиченным
манифестом, требует правило для каждой ушедшей идентичности и отвергает любое
повторное использование. Запускайте его в изменении, которое переименовывает,
удаляет или переносит идентичность, и как шлюз проверки в CI.

## Проверенные исходные пути

- `BuildTools/migration_guard.py`
- `BuildTools/fo_formats.py`
- `BuildTools/tests/test_migration_guard.py`
- `BuildTools/HelperCliInterface.json`
- `BuildTools/codegen.py`
- `Source/Tools/MetadataBaker.cpp`
- `Source/Tools/ProtoBaker.cpp`
- `Source/Common/EngineBase.cpp`
- `Source/Common/Properties.cpp`
- `Source/Common/PropertiesSerializer.cpp`
- `Source/Common/EntityProperties.h`
- `Source/Common/Common.h`
- `Docs/en/how-to/project/data-migrations.md`

## Что проверяет защита

| Идентичность | Кем объявляется | Удаление покрывает | Повторное использование |
| --- | --- | --- | --- |
| `proto <Type> <Name>` | секция прототипа в пакете со сборщиком ресурсов `Proto` (`$Name` или имя файла) либо настроенная файловая идентичность | `MigrationRule Proto <Type> Rename`, `Remove` или `Transform` | имя выведено из употребления или является источником `Rename`/`Remove`; источник `Transform` может остаться, только если уже был живым |
| `property <Owner> <Name>` | `///@ Property` в скриптовых или нативных исходниках, `FO_ENTITY_PROPERTY` в нативных исходниках | `MigrationRule Property <Owner> Rename` или `Remove` | имя выведено из употребления или является источником `Rename`/`Remove` |
| `enum <Enum> <Value>` | `///@ Enum` в скриптовых или нативных исходниках, классы `///@ ExportEnum` в нативных исходниках | `MigrationRule Enum <Enum> <Old> <New>` | значение выведено из употребления или является источником правила `Enum` |

Правила читаются так же, как их читает `MetadataBaker`: из любого файла `.fos`
или `.cs` пакета со сборщиком ресурсов `Metadata`, поэтому проекты на
AngelScript и Managed проверяются одинаково. Поддерживаются источники в кавычках
(`"Local knowledge"`), имена компонентов через точку (`Grid.ToMap`) и мигратор
вида `Namespace::Function` или `Class.Function`. Правило, записанное для
регистратора полей RefType (`<RefType>RefType`), засчитывается этому RefType.
Грамматика правил и поведение среды выполнения описаны в
[Миграции сохранённых данных и контента](data-migrations.md#объявляйте-правило-для-каждого-переименования-удаления-и-переноса).

Защита также сообщает о том, что сборка ресурсов отвергла бы и так (дубликаты
id прототипов, имена с пробельными символами, неверные правила), потому что
такой контент искажает набор идентичностей. Цепочка переименований
`A -> B -> C` корректна, если `C` существует: движок следует цепочкам
переименований до конца.

## Область проверки

По умолчанию область — это то, что видят сборщики ресурсов в главной
конфигурации:

- файлы прототипов: входы каждого пакета ресурсов, в `Bakers` которого есть
  `Proto`, отфильтрованные его шаблонами и `Baking.ProtoFileExtensions` и
  обойдённые по правилам пропуска движка (см.
  [Чтение форматов контента движка](reading-engine-formats.md#правила-которые-воспроизводит-библиотека));
- скриптовые исходники: входы `.fos` и `.cs` каждого пакета, в `Bakers`
  которого есть `Metadata`;
- `sub_config` применяется первым, поэтому его `Baking.IgnoreInputDirs`
  действует.

Нативные исходники подключаются явно: защита читает их, только если они указаны
в `native_sources`. Перечислите нативные объявления движка и проекта, если
проект хочет видеть изменения идентичностей при обновлении движка на ревью.
Файлы, которые игнорирует git, по умолчанию пропускаются, поэтому локальный
запуск видит то же дерево, что и чистый checkout в CI; пути внутри submodule
проверяются в репозитории этого submodule.

`--content-dir`, `--proto-extension` и `--script-dir` (или `content_dirs`,
`proto_extensions`, `script_dirs`) заменяют область, выведенную из пакетов, для
проекта, чью конфигурацию нельзя прочитать на хосте проверки.

## Настройка

Конфигурация — объект JSON; все ключи необязательны, кроме пути к манифесту,
который нужно задать здесь или через `--manifest`. Относительные пути
разрешаются от `--root` (по умолчанию — текущий каталог). Полный пример:

```json
{
  "main_config": "MyGame.fomain",
  "sub_config": "Validation",
  "manifest": "Tools/MigrationGuard/identities.tsv",
  "skip_git_ignored": true,
  "native_sources": ["Engine/Source/**/*.h", "SourceExt/**/*.h"],
  "exclude": ["Engine/Source/Tests/**", "SourceExt/ThirdParty/**"],
  "file_identities": [{"type": "Dialog", "patterns": ["Dialogs/**/*.fodlg"]}]
}
```

| Ключ | Значение |
| --- | --- |
| `main_config` | Главный файл конфигурации. Без него и без явных каталогов используется единственный `*.fomain` в корне. |
| `sub_config` | Имя `[SubConfig]`, применяемой до чтения пакетов ресурсов. |
| `manifest` | Закоммиченный файл манифеста. |
| `skip_git_ignored` | `true` (по умолчанию) пропускает файлы, игнорируемые git, и требует git; вне репозитория задайте `false`. |
| `native_sources` | Glob-шаблоны нативных заголовков и исходников относительно корня. Начинайте каждый шаблон с буквального каталога: обходится только он. |
| `exclude` | Glob-шаблоны относительно корня, исключаемые из любой области, для тестовых фикстур и стороннего кода. |
| `file_identities` | Идентичности проекта, хранимые по имени файла, каждая с типом прототипа, который используют её правила. |
| `content_dirs`, `proto_extensions`, `script_dirs` | Явная область вместо пакетов главной конфигурации. |

Неизвестный ключ, значение неверного типа или нечитаемый манифест являются
ошибкой конфигурации.

## Запуск

Создайте манифест один раз, закоммитьте его и затем проверяйте каждое изменение.

Windows:

```powershell
py -3 Engine\BuildTools\migration_guard.py --config Tools\MigrationGuard\migration-guard.json --update-manifest
py -3 Engine\BuildTools\migration_guard.py --config Tools\MigrationGuard\migration-guard.json --check
```

Linux:

```bash
python3 Engine/BuildTools/migration_guard.py --config Tools/MigrationGuard/migration-guard.json --update-manifest
python3 Engine/BuildTools/migration_guard.py --config Tools/MigrationGuard/migration-guard.json --check
```

В отдельном checkout движка команда
`py -3 BuildTools\migration_guard.py --main-config Examples\MinimalMultiplayer\FOnlineMinimalMultiplayer.fomain --manifest <path> --update-manifest`
проверяет инструмент на примере. macOS не проверялась.

| Код выхода | Значение |
| --- | --- |
| `0` | `--check`: дерево совпадает с манифестом. `--update-manifest`: манифест записан. |
| `1` | Нарушения, отсутствующий манифест для `--check` или отказ в обновлении из-за блокирующего нарушения. |
| `2` | Ошибка использования или конфигурации, нечитаемый ввод или недоступный git при включённом `skip_git_ignored`. |

## Чтение диагностики

Каждое нарушение — одна строка с полями через табуляцию: код, вид, владелец,
имя, сообщение и, если известно, `path:line`.

```text
MIGRATION GUARD: violations found:
  MISSING_RULE	proto	Item	OldGun	prototype id left the tree without a rule; add ///@ MigrationRule Proto Item Rename OldGun <New>, Remove OldGun or Transform OldGun <Function>
  STALE_MANIFEST	proto	Item	NewGun	prototype id is declared but not recorded live; run --update-manifest	Items/Guns.fopro:5
```

| Код | Причина | Исправление |
| --- | --- | --- |
| `MISSING_RULE` | Живая идентичность ушла из дерева без покрывающего правила. | Добавьте показанное правило или верните идентичность. |
| `REUSED_PROTO_ID`, `REUSED_PROPERTY_NAME`, `REUSED_ENUM_VALUE` | Объявленная идентичность выведена из употребления или является источником выводящего правила. | Дайте новому смыслу новое имя. |
| `DANGLING_TARGET` | Цепочка `Proto Rename` заканчивается прототипом, которого нет. | Направьте правило на живой прототип. |
| `INVALID_RULE`, `INVALID_TAG`, `INVALID_CONTENT` | Неверное правило или тег либо контент, который отвергает сборщик ресурсов. | Исправьте исходник по указанному месту. |
| `STALE_MANIFEST` | Манифест не записывает новую живую или новую выведенную из употребления идентичность. | Запустите `--update-manifest` в том же изменении. |

`--update-manifest` пишет файл, только когда не осталось блокирующих нарушений:
он никогда не записывает отсутствующее правило или повторное использование,
даже при первом запуске.

## Ведение манифеста

Манифест — отсортированный текстовый файл UTF-8, по одной строке
`<kind>\t<status>\t<owner>\t<name>` на идентичность; `status` равен `live` или
`retired`.

```text
# FOnline migration guard manifest v1: kind, status, owner, name
enum	live	QuestState	Failed
enum	retired	QuestState	Aborted
property	live	Critter	QuestStage
property	retired	Critter	OldQuestStage
proto	live	Item	Pistol
proto	retired	Item	OldPistol
```

Выведенная из употребления идентичность никогда не покидает манифест, даже
после удаления её правила по политике хранения проекта, поэтому имя остаётся
зарезервированным и после удаления надгробного правила. У защиты нет списка
исключений: идентичность, которую нужно использовать повторно, — это
проектное решение для миграции данных с новым именем. Просматривайте diff
манифеста вместе с изменением; это запись того, что изменение вывело из
употребления.

При переименовании или удалении идентичности:

1. Переименуйте или удалите её в контенте, скриптах и настройках.
2. Добавьте `MigrationRule` в модуль миграций проекта.
3. Запустите `--check`; остаться должны только строки `STALE_MANIFEST`.
4. Запустите `--update-manifest` и закоммитьте манифест вместе с изменением.

## Подключение к локальной задаче и CI

Защита читает только исходники, поэтому запускается до сборки ресурсов и не
требует build. Локальная задача может вызывать её с теми же аргументами, что и
CI, например как задача VS Code типа process:

```json
{
  "label": "Validate :: Migration Guard",
  "type": "process",
  "command": "py",
  "args": ["-3", "Engine/BuildTools/migration_guard.py", "--config", "Tools/MigrationGuard/migration-guard.json", "--check"],
  "problemMatcher": []
}
```

В CI запускайте `--check` в job проверки на полном checkout проекта (вместе с
submodule движка, если настроены нативные исходники):

```text
- name: Guard identity migrations
  run: python3 Engine/BuildTools/migration_guard.py --config Tools/MigrationGuard/migration-guard.json --check
```

Никогда не запускайте `--update-manifest` в CI; манифест меняется только в
проверенном на ревью коммите.

## Расширение в проекте

Держите тонкую обёртку проекта, которая фиксирует корень и конфигурацию, чтобы
разработчики и CI запускали одну команду:

```python
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'Engine' / 'BuildTools'))

import migration_guard  # noqa: E402


def main() -> int:
    mode = sys.argv[1:] or ['--check']
    return migration_guard.main(['--root', str(ROOT), '--config', 'Tools/MigrationGuard/migration-guard.json', *mode])


if __name__ == '__main__':
    raise SystemExit(main())
```

- Размещайте реестры идентичностей проекта рядом с защитой как отдельные
  проверки проекта: id каталогов, хранимые внутри значений свойств, текстовые
  ключи, скопированные в сохранения, имена коллекций. Читайте нужный им контент
  через `fo_formats` и дайте каждой собственный манифест или baseline с
  письменной причиной для каждой записи.
- Добавляйте идентичности, хранимые по имени файла, через `file_identities`, а
  не вторым инструментом.
- Храните модуль миграций проекта, политику хранения и решения о реестре
  идентичностей в документации проекта; защита обеспечивает их выполнение, но не
  выбирает их.

Проект, который держал собственную прежнюю защиту, переходит, перенося свою
историю выведенных из употребления имён и позволяя защите движка заново
построить живой набор. Если прежняя копия хранила такие имена строками
`retired<TAB><Owner><TAB><Name>`, добавьте вид в начало строки.

Linux:

```bash
{
  printf '# FOnline migration guard manifest v1: kind, status, owner, name\n'
  awk -F'\t' '$1 == "retired" { print "property\t" $0 }' Tools/MigrationGuard/property_name_manifest.txt
  awk -F'\t' '$1 == "retired" { print "enum\t" $0 }' Tools/MigrationGuard/enum_value_manifest.txt
} > Tools/MigrationGuard/identities.tsv
```

Windows:

```powershell
$rows = @('# FOnline migration guard manifest v1: kind, status, owner, name')
$rows += Get-Content Tools\MigrationGuard\property_name_manifest.txt -Encoding utf8 | Where-Object { $_ -like "retired`t*" } | ForEach-Object { "property`t$_" }
$rows += Get-Content Tools\MigrationGuard\enum_value_manifest.txt -Encoding utf8 | Where-Object { $_ -like "retired`t*" } | ForEach-Object { "enum`t$_" }
[System.IO.File]::WriteAllText("$PWD\Tools\MigrationGuard\identities.tsv", ($rows -join "`n") + "`n")
```

Затем запустите `--update-manifest` на дереве, на котором прежняя защита
проходила, просмотрите diff, удалите прежний инструмент и его манифесты и
переключите шаг CI. Ожидайте три отличия от самописной копии: id прототипов
в точности совпадают с id сборщика ресурсов (имя полностью именованного файла и
пустой файл не являются id, секции FixedType являются), удалённые свойства и
значения enum теперь требуют правило, как и прототипы, а выведенные из
употребления id прототипов записываются в манифест. `DANGLING_TARGET`, который
появляется только с защитой движка, означает, что сохранённые данные
мигрировали бы к прототипу, которого нет в сборке ресурсов.

## Контрольный список проверки

- Манифест закоммичен, и `--check` проходит на основной ветке.
- Конфигурация называет главную конфигурацию (и подконфигурацию, если она
  исключает тестовые входы), манифест и нативные исходники проекта.
- Каждое переименование, удаление или перенос в изменении сопровождается своим
  правилом и diff манифеста в том же коммите.
- CI запускает только `--check`; никто не запускает `--update-manifest`, чтобы
  скрыть провал.
- Реестры идентичностей проекта и их причины живут в проекте рядом с защитой.
- `py -3 -m pytest Engine/BuildTools/tests/test_migration_guard.py -q -p no:cacheprovider`
  проходит на закреплённой ревизии движка.
