---
layout: default
title: Настройка рабочей области VS Code для проекта FOnline
locale: ru
document_id: project-vscode-workspace
permalink: /Docs/ru/how-to/project/vscode-workspace.html
---

# Настройка рабочей области VS Code для проекта FOnline
<!-- docs-translation: {"document_id":"project-vscode-workspace","locale":"ru","source_path":"Docs/en/how-to/project/vscode-workspace.md","source_sha256":"fe6a517a0bdbe56a5a7ade06671c8edb87151dfa7a44884e7c470c872e352c8c"} -->
Это руководство показывает, как настроить Visual Studio Code для встраивающего
игрового проекта, который использует расширения, принадлежащие движку: FOnline
Tools и отладчик AngelScript. Оно описывает грамматику меток задач и запусков,
которую меню FOnline Tools читает из `.vscode/tasks.json` и
`.vscode/launch.json` проекта, рекомендуемый граф задач, профили запуска и
подключения (attach), а также настройки рабочей области. Оба расширения -
рабочие инструменты, которыми владеет движок; они поставляются исходниками из
`BuildTools` и устанавливаются в проект через отслеживаемые символические ссылки
рабочей области. Грамматика меток определяется исходным кодом FOnline Tools.

## Проверенные исходные пути

- `.vscode/extensions.json`
- `.gitignore`
- `.github/workflows/validate.yml`
- `BuildTools/fonline-tools/package.json`
- `BuildTools/fonline-tools/tsconfig.json`
- `BuildTools/fonline-tools/src/extension.ts`
- `BuildTools/fonline-tools/src/jsonc.ts`
- `BuildTools/fonline-tools/src/tests/jsonc.test.ts`
- `BuildTools/fonline-tools/dist/extension.js`
- `BuildTools/angelscript-debugger/package.json`
- `BuildTools/angelscript-debugger/src/extension.ts`
- `BuildTools/angelscript-debugger/src/activateFosDebug.ts`
- `BuildTools/angelscript-debugger/src/attachDiscovery.ts`
- `BuildTools/angelscript-debugger/src/fosDebug.ts`
- `BuildTools/angelscript-debugger/src/fosRuntime.ts`
- `BuildTools/angelscript-debugger/src/tests/adapter.test.ts`
- `BuildTools/angelscript-debugger/src/tests/attachDiscovery.test.ts`
- `BuildTools/tests/test_docs_debugging.py`
- `BuildTools/build.cmd`
- `BuildTools/build.sh`
- `BuildTools/buildtools.py`
- `BuildTools/cmake/helpers/Build.cmake`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/stages/Init.cmake`
- `Source/Common/Common.h`
- `Source/Common/Common.cpp`
- `Examples/MinimalProject/CMakeLists.txt`
- `Examples/MinimalProject/CMakePresets.json`
- `Examples/MinimalProject/FOnlineStarter.fomain`
- `Examples/MinimalProject/README.md`
- `Docs/en/troubleshooting/debugging.md`
- `Docs/en/reference/prototype-format/syntax.md`

## Что поставляет движок

| Элемент | Идентификатор расширения | Роль | Точка входа |
| --- | --- | --- | --- |
| FOnline Tools | `fodev.fonline-tools` | Меню быстрого выбора (quick pick), элемент строки состояния и нумерованные горячие клавиши поверх задач и конфигураций запуска рабочей области | `BuildTools/fonline-tools`, `main` указывает на `dist/extension.js` |
| FOnline AngelScript | `fodev.fo-angelscript` | Язык `fos` для `.fos`, тип отладчика `fos`, обнаружение целей для attach | `BuildTools/angelscript-debugger`, `main` указывает на `dist/extension.js` |
| Рекомендации | не расширение | `ms-vscode.cpptools` и `ms-vscode.cmake-tools` в собственном `.vscode/extensions.json` движка | `.vscode/extensions.json` |

Оба расширения помечены как `private`, имеют версию `0.1.0`, требуют VS Code
`^1.89.0` и объявляют `extensionKind` равным `workspace`. В репозитории нет VSIX
и нет записи о публикации в marketplace. Скомпилированный бандл `dist/`
закоммичен, поэтому папки расширений можно загрузить без запуска npm.

`.vscode/extensions.json` движка действует, когда открытой папкой является сам
репозиторий движка. Проекту, открытому в собственном корне, нужен свой
`.vscode/extensions.json`; см. [Рекомендуемые расширения](#рекомендуемые-расширения).

## Контракт и тестовое покрытие

FOnline Tools и отладчик AngelScript - рабочие инструменты, которыми владеет
движок. У грамматики меток FOnline Tools нет схемы и сгенерированного
справочника: спецификацией служит исходный код
`BuildTools/fonline-tools/src/extension.ts`, а его изменение может изменить
набор пунктов, которые показывает ваша рабочая область. Изменения грамматики в
движке записываются в историю изменений вместе с последствиями для существующих
`tasks.json` и `launch.json`.

Автоматическое покрытие в репозитории движка:

- `npm test` в `BuildTools/fonline-tools` проверяет типы расширения и выполняет
  unit-тесты его разборщика `tasks.json`. Построению меню, фильтрации по
  платформе и горячим клавишам нужен API VS Code, автоматических тестов у них
  нет.
- `npm test` в `BuildTools/angelscript-debugger` проверяет типы пакета,
  пересобирает `dist/`, прогоняет собранный адаптер по Debug Adapter Protocol на
  его демонстрационной среде выполнения (sample runtime) и выполняет unit-тесты
  выбора цели attach. Ни один тест не подключается к живому процессу движка и
  не выполняет UDP-обнаружение.
- Оба тестовых скрипта запускаются на машине разработчика.
  `.github/workflows/validate.yml` не запускает ни один из них; единственная
  проверка движка, затрагивающая пакеты, - `BuildTools/tests/test_docs_debugging.py`:
  она проверяет утверждения документации о `package.json` отладчика и об
  отсутствии lock-файла и VSIX. Подробности в
  [Сопровождение расширений VS Code для FOnline](../../contributing/tooling/vscode-extensions.md).

Поведение, описанное в этом руководстве, получено из исходного кода, а правила
меню и горячих клавиш проверены запуском бандла против заглушки API VS Code.
Репозиторий движка не проверяет ни одно из расширений в настоящем окне VS Code.

Контракт самого адаптера, поддерживаемые элементы управления attach и границу
безопасности см. в разделе [Нативная отладка, AngelScript и Managed C#](../../troubleshooting/debugging.md#отладчик-angelscript).

## Установка расширений

### Предварительные требования

1. Git с `core.symlinks`, установленным в `true` для клона. Проверьте командой
   `git config core.symlinks`.
2. В Windows - право создавать символические ссылки: режим разработчика
   (Developer Mode) или учётная запись с привилегией на символические ссылки.
   Тогда Git при checkout записывает настоящие ссылки.
3. VS Code `1.89` или новее.
4. Доверенная рабочая область. Ни один из `package.json` не объявляет поддержку
   недоверенных рабочих областей, поэтому доверьте папку проекта; точное
   поведение ограниченного режима VS Code не проверялось.

Без поддержки символических ссылок Git при checkout записывает символическую
ссылку как небольшой текстовый файл с целью ссылки, и VS Code не находит папку
расширения. Это поведение Git, для этого руководства оно не воспроизводилось. На
хосте Windows с разрешением на символические ссылки Git создал настоящую
символическую ссылку на каталог для отслеживаемой записи режима `120000`.

### Добавление отслеживаемых символических ссылок

Проект получает доступ к расширениям движка через две отслеживаемые
символические ссылки, поэтому каждый клон получает одни и те же папки расширений
без копирования:

```text
.vscode/extensions/fonline-tools        -> ../../Engine/BuildTools/fonline-tools
.vscode/extensions/angelscript-debugger -> ../../Engine/BuildTools/angelscript-debugger
```

Создайте их один раз средствами самого Git, что одинаково работает в Git Bash в
Windows и в bash в Linux:

```bash
mkdir -p .vscode/extensions
for name in fonline-tools angelscript-debugger; do
  blob=$(printf '%s' "../../Engine/BuildTools/$name" | git hash-object -w --stdin)
  git update-index --add --cacheinfo "120000,$blob,.vscode/extensions/$name"
  git checkout-index -f -- ".vscode/extensions/$name"
done
```

В Linux эквивалентом служит `ln -s ../../Engine/BuildTools/fonline-tools
.vscode/extensions/fonline-tools` с последующим `git add`. В PowerShell
`New-Item -ItemType SymbolicLink -Target` разрешает относительную цель
относительно текущего каталога, а не папки ссылки, поэтому созданная так ссылка
не указывает на движок. Используйте приведённый выше путь через Git.

Проверьте ссылки:

```powershell
git config core.symlinks
Get-Item .vscode\extensions\fonline-tools | Select-Object LinkType, Target
```

```bash
git config core.symlinks
ls -l .vscode/extensions
```

Если клон, сделанный без поддержки символических ссылок, оставил обычные файлы,
включите символические ссылки и восстановите записи:

```bash
git config core.symlinks true
rm .vscode/extensions/fonline-tools .vscode/extensions/angelscript-debugger
git checkout -- .vscode/extensions
```

### Проверка загрузки расширений

В движке нет кода, который загружает папки по символическим ссылкам: загрузка
каталогов из `<workspace>/.vscode/extensions` - поведение самого VS Code, и
встраивающие проекты ежедневно используют этот маршрут через символические
ссылки. Автоматического теста для него в репозитории движка нет, поэтому
подтвердите результат один раз для каждого клона и версии VS Code:

1. Откройте папку проекта и примите запрос о доверии к рабочей области.
2. Найдите пункт `FOnline Tools` у левого края строки состояния и команду
   `FOnline: Show Tools` в палитре команд.
3. Выполните `Developer: Show Running Extensions` и убедитесь, что в списке есть
   `FOnline Tools` и `FOnline AngelScript`.
4. Если они только предложены как рекомендации, установите их по запросу в
   представлении «Расширения».

Расширение `fos` активируется лениво по событиям отладки
(`onDebugResolve:fos`), поэтому в списке запущенных оно может отсутствовать, пока
не начнётся сессия отладки `fos`.

## Как FOnline Tools строит меню

Расширение активируется по `onStartupFinished`, регистрирует команду `FOnline:
Show Tools` (`fonline-tools.showTools`) и показывает элемент `FOnline Tools` у
левого края строки состояния. Щелчок по нему запускает ту же команду. Команда
открывает меню быстрого выбора, смешивающее задачи и конфигурации запуска.

### Источники данных

- Задачи читаются из `.vscode/tasks.json` только первой папки рабочей области.
  Расширение читает файл само и принимает тот же ослабленный JSON, что и VS
  Code: комментарии `//` и `/* */`, завершающие запятые перед `}` или `]` и
  метку порядка байтов (BOM) в начале. Если файла нет, в меню нет задач. Файл,
  который всё равно не разбирается, например с незакрытой скобкой, вызывает
  ошибку `FOnline Tools: Cannot parse <path>: <reason>`, и ни меню, ни горячие
  клавиши не работают, пока файл не исправлен.
- Затем расширение запрашивает у VS Code задачи рабочей области и оставляет те,
  чьё имя совпадает с меткой из этого файла, а область действия - рабочая
  область или первая папка.
- Конфигурации запуска и составные конфигурации (compounds) берутся из API
  конфигурации VS Code для `launch` в первой папке рабочей области, поэтому
  `launch.json` может содержать комментарии.
- В рабочей области с несколькими корнями (multi-root) задачи и записи запуска
  других папок никогда не перечисляются.

Задача без `label` никогда не попадает в список.

### Грамматика меток

Каждый элемент управления записывается внутри `label` задачи или `name`
запуска. Теги остаются в строке, которую хранит VS Code, поэтому `dependsOn`,
`preLaunchTask` и `configurations` составной конфигурации должны повторять метку
полностью, включая теги.

| Элемент | Запись | Эффект |
| --- | --- | --- |
| Группа | `Group :: Item` | Записи с общим `Group` попадают в подменю, показываемое как `Group ...`. Разделяет только первый `::`, и разделение требует текста с обеих сторон после удаления тегов. Метка, начинающаяся с `::`, не группируется. Уровень только один, поэтому `A :: B :: C` - это группа `A`, элемент `B :: C`. |
| Скрытая запись | `[hidden]` | Запись исключается из меню и из горячих клавиш. Задача или конфигурация по-прежнему существует для VS Code, `dependsOn`, `preLaunchTask` и составных конфигураций. |
| Тег платформы | `[windows]`, `[linux]`, `[macos]` | Запись показывается только на перечисленных платформах хоста. Несколько тегов образуют объединение. |
| Псевдонимы тегов платформы | `[win32]`, `[win64]` означают windows; `[osx]`, `[darwin]` означают macOS | Теги не зависят от регистра. |
| Тег сортировки | `[sort:N]` с цифрами | Задаёт порядок записей и привязывает горячую клавишу `Ctrl+Shift+N` для `N` от 1 до 9. |
| Квалификатор платформы | `(Windows)`, `(Linux x64)` | Группа в круглых скобках, первое слово которой - одно из названий платформ или псевдонимов выше, ограничивает хост. Прочие скобки, например `(Debug)`, игнорируются. Квалификатор остаётся в отображаемой метке. |
| Блоки по ОС | ключи `windows`, `linux`, `osx` у задачи или конфигурации | Наличие ключа отмечает платформу как поддерживаемую; см. правила приоритета ниже. |
| Тип отладчика, ограниченный хостом | `"type": "cppvsdbg"` | Конфигурация запуска этого типа показывается только в Windows. Это единственный тип, который знает расширение. |

Теги удаляются из отображаемой метки, а последовательности пробелов
сворачиваются, поэтому `Server [windows] [sort:3]` отображается как `Server`.
Отображаемые имена в `launch.json` всё равно должны быть уникальными, поэтому
варианты для разных хостов различаются тегом.

### Правила фильтрации по платформе

Хост определяется по Node `process.platform`: `win32` - это windows, `linux` -
linux, `darwin` - macOS. Любое другое значение или отсутствие `process`
отключает фильтрацию, и показываются все записи. Для одной записи решает первое
применимое правило:

1. Если у метки есть теги платформы, хост должен быть среди них.
2. Иначе, если у метки есть распознанные квалификаторы в скобках, хост должен
   быть среди них.
3. Иначе, если у записи есть любой из ключей `windows`, `linux` или `osx`, хост
   должен быть среди имеющихся ключей.
4. Иначе запись показывается.

Конфигурация запуска должна пройти ещё и правило типа отладчика: `cppvsdbg`
требует хоста Windows в дополнение к правилам меток. Составная конфигурация
использует правила меток, требует непустого списка `configurations`, а каждое
перечисленное имя должно быть обычной строкой, равной имени конфигурации запуска,
поддерживаемой на этом хосте. Поэтому составная конфигурация, ссылающаяся на
конфигурацию только для Windows, исчезает в Linux, скрытая конфигурация всё равно
считается поддерживаемой, а запись-объект `{ "name": ..., "folder": ... }`
приводит к исчезновению составной конфигурации.

Следствия, которые стоит учитывать при планировании:

- Задача, у которой есть только блок переопределения `windows`, видна только в
  Windows. Чтобы задача была видна на каждом хосте, дайте ей все три блока или
  ни одного.
- Задача с блоками `windows` и `linux` без блока `osx` скрыта в macOS; для
  проекта без пресетов macOS это ожидаемый результат.
- Составные задачи, которые только перечисляют `dependsOn`, не имеют блоков и
  остаются видимыми на каждом хосте, в том числе там, где их зависимости скрыты.
  Добавьте такой задаче теги `[windows]` и `[linux]`, если она должна следовать
  за своими зависимостями.

### Порядок и подменю

Верхний уровень сначала перечисляет прямые записи, затем записи групп. Внутри
списка порядок такой: записи с `[sort:N]` по возрастанию `N`, затем остальные по
метке с учётом локали. Записи задач получают описание `Task`, конфигурации
запуска - `Launch`, составные конфигурации - `Launch Compound`, а подменю -
`Task Group` или `Launch Group`. Ввод фильтрует по метке, описанию и детали.

Задачи и конфигурации запуска группируются раздельно. Группа задач `Build` и
группа запусков `Build` появляются как два пункта подменю. `[sort:N]` у
элемента внутри группы упорядочивает его только в пределах своего подменю.

### Горячие клавиши

`Ctrl+Shift+1` ... `Ctrl+Shift+9` запускают `FOnline: Run Sorted Item 1` ... `9`
(`fonline-tools.runSorted1` ... `runSorted9`). Они активны, когда не показан ни
виджет Quick Open, ни виджет подсказок. Если сочетание конфликтует с другим
расширением, переназначьте идентификаторы команд в `keybindings.json`. Совпадают
ли сочетания по умолчанию со штатными сочетаниями VS Code в вашей раскладке, не
проверялось.

Горячая клавиша запускает запись верхнего уровня, у которой `[sort:N]` равен
цифре:

- Записи внутри группы никогда не получают горячей клавиши. Держите цели
  горячих клавиш прямыми записями.
- У скрытых записей нет горячей клавиши.
- `N`, равное 0 или от 10 и выше, только упорядочивает меню.
- Если две записи имеют одинаковый номер, запускается та, что стоит в меню
  первой, то есть та, чья метка сортируется раньше. Используйте уникальные
  номера.
- Если ни одна запись не подходит, показывается предупреждение `No FOnline item
  with [sort:N] found.`.

Горячие клавиши не используют кэш. Каждое нажатие заново строит тот же список
верхнего уровня, который показывает `FOnline: Show Tools`, из текущего
`tasks.json`, задач, которые возвращает VS Code, и текущего `launch.json`,
поэтому правка действует со следующего нажатия без повторного открытия меню и
перезагрузки окна.

## Рекомендуемый граф задач

Держите граф небольшим и единообразным, чтобы меню оставалось читаемым, а задачи
никогда не запускались в неверном порядке:

1. Скрытая задача `Configure` выполняет шаг конфигурации CMake для пресета хоста.
   Каждая задача сборки, запекания и тестов перечисляет её в `dependsOn`.
2. Задачи сборки и запекания сгруппированы (`Build :: ...`, `Test :: ...`) и
   вызывают `cmake --build <build dir> --config <config> --target <target>` с
   явной целью.
3. `Prepare` запекает ресурсы и собирает исполняемые файлы, нужные запуску. Она
   использует `dependsOrder: sequence`, потому что шаги делят одно дерево сборки.
4. Конфигурации запуска используют `preLaunchTask`, равный метке `Prepare`.
5. Агрегирующая задача `Verify` последовательно запускает `Prepare`, а затем
   smoke- или тестовую цель проекта.
6. Задачи, управляемые интерпретатором, выбирают интерпретатор для каждого хоста,
   как показано далее.

Не допускайте параллельного запуска двух дочерних задач, пишущих в один и тот же
вывод сборки или запекания. VS Code запускает дочерние элементы `dependsOn`
параллельно, если `dependsOrder` не равен `sequence`. Когда от `Configure`
зависят несколько задач, она может выполниться в последовательности более одного
раза; она идемпотентна, но это руководство не проверяло, устраняет ли VS Code
такие дубликаты.

В качестве конкретных целей используйте имена, которые генерирует ваш проект.
Пример ниже следует минимальному проекту, принадлежащему движку: имена пресетов
`windows` и `linux`, каталог сборки `Build/<preset>`, имя разработки `FOSTART`,
цели `BakeResources`, `FOSTART_ServerHeadless` и `RunStarterSmoke`.

### Выбор интерпретатора Python для каждой ОС

Не ставьте голый `python` в общую задачу. На рабочем хосте Windows это имя может
указывать на Python 2, а скрипты BuildTools движка под Python 2 не разбираются.
`BuildTools/build.cmd` предпочитает лаунчер `py -3` и откатывается на `python`.
`BuildTools/build.sh` предпочитает `python3` и откатывается на `python`.
Повторите это в задачах с блоками по ОС: `py` с первым аргументом `-3` в Windows
и `python3` в Linux. У такой задачи есть блоки `windows` и `linux`, поэтому в
macOS она не показывается.

BuildTools предлагает `buildtools.py host-check {linux,macos,windows}` и
`buildtools.py build <platform> <target> [config]`; см.
[справочник команд BuildTools](../../reference/buildtools/commands.md).

## Профили запуска

Держите три вида профилей, все они управляются из `launch.json`:

- Нативный запуск под отладчиком с `preLaunchTask`. Используйте `cppvsdbg` в
  Windows и `cppdbg` с GDB или LLDB в Linux; оба относятся к расширению
  `ms-vscode.cpptools`, которое владеет их полями. Укажите в `program` точный
  сгенерированный исполняемый файл, а в `cwd` - каталог, который использует
  сборка. Приложения движка собираются с рабочим каталогом, равным выходному
  пути (`FO_OUTPUT_PATH`); минимальный проект задаёт его равным двоичному
  каталогу CMake.
- Подключение к AngelScript с типом `fos` и запросом `attach`. Поля:
  `endpoint`, `discoveryPort`, `discoveryTimeoutMs` и `processId`.
- Составная конфигурация из двух предыдущих, если нужно запустить процесс и
  подключиться одним действием.

Правила, специфичные для адаптера `fos`, выведены из его исходного кода:

- Используйте `fos` только для `attach`. Его запрос `launch` запускает
  демонстрационную среду выполнения адаптера, которая читает `program` как
  текстовый файл; процесс движка он не запускает.
- Процесс должен работать с `AngelScript.DebuggerEnabled`, равным true. Пример
  передаёт `--AngelScript.DebuggerEnabled True` в командной строке; справочник
  по настройке и границу безопасности см. в разделе
  [Отладчик AngelScript](../../troubleshooting/debugging.md#отладчик-angelscript).
- Если `endpoint` не задан, адаптер выполняет UDP-обнаружение. Без `processId`
  он показывает список всех обнаруженных целей для выбора и сохраняет выбранные
  `endpoint` и `processId`. `processId` в `launch.json` фильтрует обнаруженные
  цели по точному совпадению с объявленным идентификатором `<pid>:<tcp-port>`,
  поэтому голый идентификатор процесса ничему не соответствует: при одном
  совпадении attach выполняется без выбора, несколько совпадений (один и тот же
  идентификатор с нескольких хостов) предлагаются в списке, а при отсутствии
  совпадений показывается предупреждение и сессия отменяется. Задайте `endpoint`
  (например, `tcp://127.0.0.1:43042`), чтобы пропустить обнаружение.
- Обнаружение выполняется при старте сессии attach. В составной конфигурации,
  которая ещё и запускает процесс, endpoint к этому моменту уже должен быть
  открыт, иначе обнаружение сообщит об отсутствии целей. Тайминг старта
  составных конфигураций в VS Code здесь не проверялся.

Поля, матрица возможностей, правила безопасности и checklist сопровождаемого
профиля описаны в разделах
[Интеграция отладчика во встраивающем проекте](../../troubleshooting/debugging.md#интеграция-отладчика-во-встраивающем-проекте)
и [Checklist launch-профиля проекта](../../troubleshooting/debugging.md#checklist-launch-профиля-проекта).
Не копируйте их в документацию проекта.

## Полный минимальный пример

Два файла ниже записаны обычным JSON; комментарии и завершающие запятые тоже
были бы приняты. Замените имена пресетов, каталогов и целей
на имена вашего проекта. Ввод `serverExecutable` запрашивает путь к исполняемому
файлу: для сборки Release движок помещает серверное приложение в
`<FO_OUTPUT_PATH>/Binaries/Server-<FO_BUILD_PLATFORM>/<DevName>_ServerHeadless`,
где платформа - `Windows-win64` на 64-разрядной Windows, а суффикс - `.exe`. Этот
путь выведен из исходников CMake, а не наблюдался в сборке. Замените ввод
буквальным путём, когда подтвердите его в своём дереве сборки. Аргумент
`-ApplySubConfig StarterSmoke` выбирает подконфигурацию минимального проекта;
используйте свою.

`.vscode/tasks.json`:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Configure [hidden]",
      "type": "shell",
      "command": "cmake",
      "windows": { "args": ["--preset", "windows"] },
      "linux": { "args": ["--preset", "linux"] },
      "problemMatcher": []
    },
    {
      "label": "Build :: Bake Resources",
      "type": "shell",
      "command": "cmake",
      "windows": { "args": ["--build", "Build/windows", "--config", "Release", "--target", "BakeResources"] },
      "linux": { "args": ["--build", "Build/linux", "--config", "Release", "--target", "BakeResources"] },
      "dependsOn": ["Configure [hidden]"],
      "problemMatcher": []
    },
    {
      "label": "Build :: Server Headless",
      "type": "shell",
      "command": "cmake",
      "windows": { "args": ["--build", "Build/windows", "--config", "Release", "--target", "FOSTART_ServerHeadless"] },
      "linux": { "args": ["--build", "Build/linux", "--config", "Release", "--target", "FOSTART_ServerHeadless"] },
      "dependsOn": ["Configure [hidden]"],
      "problemMatcher": []
    },
    {
      "label": "Test :: Starter Smoke",
      "type": "shell",
      "command": "cmake",
      "windows": { "args": ["--build", "Build/windows", "--config", "Release", "--target", "RunStarterSmoke"] },
      "linux": { "args": ["--build", "Build/linux", "--config", "Release", "--target", "RunStarterSmoke"] },
      "dependsOn": ["Configure [hidden]"],
      "problemMatcher": []
    },
    {
      "label": "Tools :: Host Check",
      "type": "shell",
      "windows": { "command": "py", "args": ["-3", "Engine/BuildTools/buildtools.py", "host-check", "windows"] },
      "linux": { "command": "python3", "args": ["Engine/BuildTools/buildtools.py", "host-check", "linux"] },
      "problemMatcher": []
    },
    {
      "label": "Prepare [sort:1]",
      "dependsOn": ["Build :: Bake Resources", "Build :: Server Headless"],
      "dependsOrder": "sequence"
    },
    {
      "label": "Verify [sort:2]",
      "dependsOn": ["Prepare [sort:1]", "Test :: Starter Smoke"],
      "dependsOrder": "sequence"
    }
  ]
}
```

`.vscode/launch.json`:

```json
{
  "version": "0.2.0",
  "inputs": [
    {
      "id": "serverExecutable",
      "type": "promptString",
      "description": "Path to the built FOSTART_ServerHeadless executable"
    }
  ],
  "configurations": [
    {
      "name": "Server [windows] [sort:3]",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${input:serverExecutable}",
      "args": ["-ApplyConfig", "${workspaceFolder}/FOnlineStarter.fomain", "-ApplySubConfig", "StarterSmoke"],
      "cwd": "${workspaceFolder}/Build/windows",
      "preLaunchTask": "Prepare [sort:1]"
    },
    {
      "name": "Server [linux] [sort:3]",
      "type": "cppdbg",
      "request": "launch",
      "MIMode": "gdb",
      "program": "${input:serverExecutable}",
      "args": ["-ApplyConfig", "${workspaceFolder}/FOnlineStarter.fomain", "-ApplySubConfig", "StarterSmoke"],
      "cwd": "${workspaceFolder}/Build/linux",
      "preLaunchTask": "Prepare [sort:1]"
    },
    {
      "name": "Server With Script Debugger [windows] [hidden]",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${input:serverExecutable}",
      "args": ["-ApplyConfig", "${workspaceFolder}/FOnlineStarter.fomain", "-ApplySubConfig", "StarterSmoke", "--AngelScript.DebuggerEnabled", "True"],
      "cwd": "${workspaceFolder}/Build/windows",
      "preLaunchTask": "Prepare [sort:1]"
    },
    {
      "name": "Server With Script Debugger [linux] [hidden]",
      "type": "cppdbg",
      "request": "launch",
      "MIMode": "gdb",
      "program": "${input:serverExecutable}",
      "args": ["-ApplyConfig", "${workspaceFolder}/FOnlineStarter.fomain", "-ApplySubConfig", "StarterSmoke", "--AngelScript.DebuggerEnabled", "True"],
      "cwd": "${workspaceFolder}/Build/linux",
      "preLaunchTask": "Prepare [sort:1]"
    },
    {
      "name": "Attach AngelScript [sort:5]",
      "type": "fos",
      "request": "attach",
      "discoveryPort": 43001,
      "discoveryTimeoutMs": 800
    }
  ],
  "compounds": [
    {
      "name": "Server And Script Attach [windows] [sort:4]",
      "configurations": ["Server With Script Debugger [windows] [hidden]", "Attach AngelScript [sort:5]"]
    },
    {
      "name": "Server And Script Attach [linux] [sort:4]",
      "configurations": ["Server With Script Debugger [linux] [hidden]", "Attach AngelScript [sort:5]"]
    }
  ]
}
```

Получившееся меню на хосте Windows или Linux: на верхнем уровне `Prepare`,
`Verify`, `Server`, `Server And Script Attach` и `Attach AngelScript`, затем
подменю `Build ...`, `Test ...` и `Tools ...`. Горячие клавиши `Ctrl+Shift+1` ...
`Ctrl+Shift+5` запускают эти пять записей в указанном порядке. В macOS остаются
только `Prepare`, `Verify` и `Attach AngelScript`, причём `Prepare` завершится
ошибкой, так как её зависимости там скрыты: если это важно, добавьте теги
`[windows] [linux]` к двум составным меткам и обновите все ссылки `dependsOn` и
`preLaunchTask` соответственно.

Составная задача, у которой есть только `dependsOn`, попадает в список
расширения лишь в том случае, если VS Code возвращает её в ответ на запрос
задач. В настоящем окне VS Code это не проверялось.

## Настройки рабочей области и расширения

### Рекомендуемые расширения

`.vscode/extensions.json` самого движка рекомендует `ms-vscode.cpptools`
(нативная отладка, предоставляет `cppvsdbg` и `cppdbg`) и
`ms-vscode.cmake-tools` (пресеты CMake). Повторите их в `.vscode/extensions.json`
проекта:

```json
{
  "recommendations": [
    "ms-vscode.cpptools",
    "ms-vscode.cmake-tools"
  ]
}
```

Не перечисляйте там `fodev.fonline-tools` и `fodev.fo-angelscript`: это не
расширения из marketplace, они доставляются символическими ссылками.

### `settings.json` рабочей области

Предлагаемые настройки. Первый блок сопоставляет текстовые форматы FOnline,
использующие общий синтаксис конфигурации (`.fomain`, прототипы `.fopro`, карты
`.fomap`, комментарии `#`), с режимом INI в VS Code. Не сопоставляйте `.fotxt` -
формат с фигурными скобками без синтаксиса комментариев. Не переназначайте `.fos`,
который занимает расширение AngelScript. Списки исключений повторяют
сгенерированные деревья, которые минимальный проект игнорирует в Git.
`cmake.useCMakePresets` - настройка CMake Tools; поведение этого расширения в
руководстве не проверялось.

```json
{
  "files.associations": {
    "*.fomain": "ini",
    "*.fopro": "ini",
    "*.fomap": "ini"
  },
  "files.watcherExclude": {
    "**/Build/**": true,
    "**/Baking/**": true,
    "**/Cache/**": true,
    "**/Binaries/**": true,
    "**/Workspace/**": true
  },
  "search.exclude": {
    "**/Build/**": true,
    "**/Baking/**": true,
    "**/Cache/**": true,
    "**/Binaries/**": true,
    "**/Workspace/**": true
  },
  "cmake.useCMakePresets": "always"
}
```

Добавьте в `files.associations` дополнительные расширения файлов прототипов вашего
проекта, если он расширяет `Baking.ProtoFileExtensions`. Не добавляйте
`Engine/ThirdParty` в `search.exclude`, только если вы намеренно ищете по
сторонним библиотекам.

## Visual Studio, Natvis и другие IDE

- Visual Studio: сгенерированное решение MSVC автоматически подключает файлы
  Natvis и NatJMC движка, а цели приложений задают рабочий каталог отладчика
  равным `FO_OUTPUT_PATH`. См. [Визуализаторы Visual Studio](../../troubleshooting/debugging.md#визуализаторы-visual-studio).
  Как подключить файлы Natvis к сессии `cppvsdbg` в VS Code, не проверялось.
- Нативная отладка в Linux и macOS использует GDB или LLDB с точным исполняемым
  файлом; движок не заявляет проверенного профиля редактора для macOS. См.
  [отладку по платформам](../../troubleshooting/debugging.md#windows).
- JetBrains и другие редакторы: `.gitignore` движка исключает локальное
  состояние `.idea`, `.theia`, `.vs` и `.cache`, но проверенного профиля для них
  не поставляет. Протокол attach для AngelScript описан в руководстве по отладке
  для любого клиента, который его реализует; сам адаптер `fos` работает только в
  VS Code.

## Контрольный список проверки

1. `git config core.symlinks` выводит `true`, а обе ссылки на расширения
   разрешаются в папки движка.
2. `FOnline Tools` появляется в строке состояния, а `FOnline: Show Tools`
   перечисляет ожидаемые записи для текущей ОС.
3. При открытии `FOnline: Show Tools` нет ошибки `Cannot parse` для
   `tasks.json`, а каждое имя в `dependsOn`, `preLaunchTask` и составных
   конфигурациях совпадает с меткой символ в символ, включая теги.
4. Скрытая задача `Configure` является зависимостью каждой задачи сборки,
   запекания и тестов.
5. `Prepare` запекает и собирает последовательно; `Verify` запускает `Prepare`, а
   затем smoke-цель.
6. Каждая цель горячей клавиши - прямая запись с `[sort:N]` от 1 до 9, и ни одна
   не скрыта.
7. Задачи Python используют `py -3` в Windows и `python3` в Linux.
8. Нативный запуск останавливается на точке останова, а attach `fos` находит
   процесс и останавливается на точке останова скрипта, согласно
   [checklist launch-профиля](../../troubleshooting/debugging.md#checklist-launch-профиля-проекта).

## См. также

- [Сопровождение расширений VS Code для FOnline](../../contributing/tooling/vscode-extensions.md)
- [Нативная отладка, AngelScript и Managed C#](../../troubleshooting/debugging.md)
- [Встраивание FOnline в игровой проект](../build/embedding-project.md)
- [Подготовка рабочей станции для проекта FOnline](workstation-setup.md)
- [Контроль качества: проверки](quality-gates.md)
- [Непрерывная интеграция](continuous-integration.md)
- [Тесты проекта](project-tests.md)
- [Индекс жизненного цикла проекта](index.md)
