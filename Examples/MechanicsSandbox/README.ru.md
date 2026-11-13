# Песочница механик FOnline

Эта принадлежащая Engine фикстура содержит исполняемый исходный код рецептов механик: один общий bootstrap, по одному модулю скрипта на рецепт и серверные тесты, которые выполняются через копируемый тестовый harness скриптов. AngelScript проверен во время выполнения (runtime-verified); файлы Managed C# рядом с ним повторяют те же объявления в виде формы (shape), которую CI Engine разбирает, но не компилирует. Страницы рецептов в `Docs/en/how-to/mechanics/` цитируют эти файлы дословно, а `recipes.json` служит машиночитаемым индексом, который связывает каждую страницу с её файлами, символами Engine и сценариями smoke.

## Статус рецептов

| Рецепт | Страница | AngelScript | Managed C# | Сценарии smoke |
|---|---|---|---|---|
| Собственное сохраняемое состояние персонажа | `Docs/en/how-to/mechanics/persistent-state.md` | runtime-verified | shape | `persistent-state`, `persistent-state-restart-write`, `persistent-state-restart-verify` |
| Проверяемый запрос действия | `Docs/en/how-to/mechanics/validated-action.md` | runtime-verified | shape | `validated-action` |
| Временный эффект, переживающий перезапуск | `Docs/en/how-to/mechanics/timed-effect.md` | runtime-verified | shape | `timed-effect`, `timed-effect-restart-write`, `timed-effect-restart-verify` |
| Блокировка нескольких сущностей для одного действия (сквозная тема) | `Docs/en/how-to/mechanics/cover-transactions.md` | runtime-verified | shape | `cover-transactions` |

`runtime-verified` означает, что сценарий прошёл на собранном и запечённом сервере (и на headless-клиенте, где он указан). `shape` означает, что файл C# объявляет те же метаданные `///@` и повторяет поток управления, но ни одна managed-сборка его не компилировала и не запускала.

## Структура

```text
Examples/MechanicsSandbox/
├── CMakeLists.txt, CMakePresets.json   build, bake and the RunMechanicsChecks target
├── FOnlineMechanicsSandbox.fomain      generated settings, harness values, sub-configs, resource packs
├── generate_config.py                  writes the .fomain from Engine defaults plus sandbox overrides
├── recipes.json                        recipe -> page, files, Engine symbols, smoke scenarios
├── mechanics-smoke.json                gameplay_test_runner manifest, one or more scenarios per recipe
├── run_mechanics_smoke.py              clears the restart databases and runs the manifest
├── validate.py                         configure, build, bake and run on this host
├── Content/                            Sandbox.fopro plus one prototype file per recipe
├── Maps/Sandbox_Map.fomap              the one map every test location uses
├── Scripts/Sandbox.fos                 shared world creation and headless-client login
├── Scripts/Testing.fos                 byte-identical copy of the GameplayTestHarness core
├── Scripts/Recipes/<Name>.fos          one module per recipe; recipes never call each other
├── Scripts/Tests/Test_<Name>.fos       harness suites for that recipe
└── Managed/Recipes/<Name>.cs           Managed C# shape with the same declarations
```

Пространство имён модуля совпадает с именем его файла без расширения. По этому правилу бэкенд AngelScript связывает обработчики удалённых вызовов, а `BuildTools/check_angelscript_hygiene.py` проверяет его для каждого примера, поэтому пространства имён рецептов называются `PersistentState`, `ValidatedAction`, `TimedEffect` и `CoverTransaction`.

## Запуск проверок

Пример ожидает Engine в каталоге `Engine/` рядом с `CMakeLists.txt`. Внутри репозитория Engine создайте эту ссылку один раз; `.gitignore` её исключает. Удаляйте её потом командой `cmd /c rmdir Engine` в Windows или `rm Engine` в Linux, но никогда рекурсивным удалением.

Windows (PowerShell), из корня репозитория Engine:

```powershell
cd Examples\MechanicsSandbox
New-Item -ItemType Junction -Path Engine -Target ..\..
python validate.py
```

Linux (bash), из корня репозитория Engine:

```bash
cd Examples/MechanicsSandbox
ln -s ../.. Engine
python3 validate.py
```

`validate.py` конфигурирует пресет хоста в `Build/<preset>/` и собирает цель `RunMechanicsChecks`. Эта цель собирает headless-сервер и headless-клиент, проверяет, что сгенерированная конфигурация актуальна, выполняет принудительное запекание, а затем запускает `run_mechanics_smoke.py` в каталоге сборки. Конфигурация задаёт `Baking.PrebakeOnStartup = False`, поэтому процессы сценариев стартуют на этом одном запекании, а не запекают ресурсы заново каждый. Обёртка также создаёт одноразовую пару ключей защищённого канала в игнорируемом каталоге `Workspace/` рядом с конфигурацией: каждому серверу нужен `ServerNetwork.ChannelSecretKey`, даже при выключенной сети, а конфигурация читает обе половины через `$TARGET_FILE{...}`. Этот ключ служит локальной тестовой идентичностью и никогда не используется в production. Передайте `--binary-dir <directory>`, чтобы использовать другое дерево сборки. Запуск успешен, когда runner печатает `status=passed` для каждого сценария и завершается с кодом 0; JSON-отчёт записывается в `mechanics-smoke-report.json` в каталоге сборки.

Во время разработки запускайте один набор тестов на уже запечённых ресурсах; сначала один раз запустите обёртку, чтобы появился ключ канала. Windows (PowerShell), из каталога сборки:

```powershell
& ".\Binaries\Server-Windows-win64\FOMECH_ServerHeadless.exe" -ApplyConfig "<sandbox>\FOnlineMechanicsSandbox.fomain" -ApplySubConfig RecipeTests -Testing.Filter "timed_effect."
```

Linux (bash), из каталога сборки:

```bash
./Binaries/Server-Linux-x64/FOMECH_ServerHeadless -ApplyConfig <sandbox>/FOnlineMechanicsSandbox.fomain -ApplySubConfig RecipeTests -Testing.Filter "timed_effect."
```

## Как сценарии доказывают каждый рецепт

- **Тесты внутри процесса** используют sub-config `RecipeTests`: harness включён, сеть выключена, `Server.DbStorage = Memory`. База данных в памяти хранит записи всё время жизни процесса, поэтому `Game.UnloadCritter` с последующим `Game.LoadCritter` воспроизводят восстановление без перезапуска.
- **Тесты перезапуска** используют sub-config `RecipeRestart` и выполняются как два сценария на одном каталоге JSON-базы данных. Фаза 1 проверяет, что каталог пуст, создаёт и сохраняет состояние и завершает работу; фаза 2 запускает новый процесс сервера на том же каталоге и проверяет, что вернулось. Манифест передаёт `-Server.DbStorage "JSON {restart_db}/<recipe>"`, а `run_mechanics_smoke.py` удаляет `{restart_db}` перед запуском, поэтому фаза 1 всегда начинается с пустой базы.
- **Проверяемое действие** использует настоящий headless-клиент. AngelScript запрещает прямой вызов обработчика `[[ServerRemoteCall]]`, а подмена пропустила бы именно то, ради чего существует рецепт: ограничение размера полезной нагрузки, предоставленный Engine объект `Player` и покрытие диспетчера для этого игрока и его персонажа. Серверный тест создаёт цели и отправляет клиенту одну серию запросов: устройство на другой карте, устройство другого владельца, устройство вне досягаемости, допустимое устройство и снова допустимое устройство, когда энергии уже не осталось. Клиент отправляет пять вызовов `ActivateDevice`, а затем `DeviceDrillSent`; обработчики одного соединения выполняются в порядке поступления и ни один из них не приостанавливается, поэтому последний вызов видит итоговое состояние. Манифест требует каждую причину отказа в журнале сервера, а тест проверяет, что изменил что-либо только допустимый запрос.
- **Временный эффект** содержит в `Scripts/Tests/Test_TimedEffect.fos` наивный вариант как отрицательный контроль: его таймер запускается только в сеттере свойства. Тесты показывают, что выгрузка и загрузка, а также полный перезапуск возвращают оба срока окончания, но только таймер рецепта, и что наивный эффект после этого остаётся активным после своего срока.
- **Восстановление мира и `Game.LoadCritter`.** Персонаж карты, восстановленный при запуске сервера, получает `Game.OnCritterInit(cr, false)`, но не `Game.OnCritterLoad`; оба события вызывает только `Game.LoadCritter`. Тесты сохраняемого состояния считают оба события на каждом пути, поэтому повторный запуск таймера во временном эффекте выполняется в `OnCritterInit`.
- **Снимки сервера здесь не проверяются.** Восстановление из снимка возвращает сохранённые события времени после того, как события инициализации уже выполнены (`ServerEngine::RestoreSnapshotTimeEvents` в `Source/Server/Server.cpp`), поэтому проверка `CountTimeEvent` при инициализации их не видит, и у персонажа может оказаться два таймера. Поэтому обработчик окончания заново читает срок и останавливает таймер, который застал эффект уже завершённым. Песочница не выполняет восстановление из снимка; этот путь установлен чтением исходного кода.

## Добавление рецепта

1. Добавьте `Scripts/Recipes/<Name>.fos` с `namespace <Name>`, объявлениями `///@` рецепта и его серверной и клиентской частями. Используйте только API Engine и помощники `Sandbox::`; не вызывайте другой рецепт.
2. Добавьте `Content/<Name>.fopro` с его прототипами и текстами `engl` и `russ`.
3. Добавьте `Scripts/Tests/Test_<Name>.fos`. Регистрируйте тесты из `Game.OnTestingInit` в наборе, названном по рецепту, например `my_recipe.does_the_thing`; фазы перезапуска используют наборы `<suite>_restart_write` и `<suite>_restart_verify`.
4. Добавьте сценарии в `mechanics-smoke.json` с `-Testing.Filter "<suite>."` и одним обязательным маркером `[Testing] PASS <suite>.<name>` на каждый тест.
5. Добавьте `Managed/Recipes/<Name>.cs` с теми же строками `///@`.
6. Добавьте запись в `recipes.json`: файлы, идентификаторы символов из `Docs/generated/api.json`, метаданные проекта, встроенные средства бэкенда и сценарии smoke.
7. Запустите `python -m pytest BuildTools/tests/test_mechanics_sandbox.py -q -p no:cacheprovider` из корня Engine, а затем полную проверку, описанную выше.

## Примечания

- `FOnlineMechanicsSandbox.fomain` генерируется. Запуск без пакета применяет `.fomain` к обнулённым настройкам, поэтому файл перечисляет все настройки Engine; меняйте `OVERRIDES` или `PROJECT_SECTIONS` в `generate_config.py`, запускайте `python generate_config.py` и полагайтесь на `--check` (он выполняется перед каждым запеканием), чтобы обнаружить устаревший файл. Настройки-перечисления записываются числами, потому что среда выполнения читает их как целые.
- Персонаж карты, сохраняемый только потому, что сохраняема его локация, теряет эту сохраняемость и свою запись в базе данных, когда покидает карту. `Game.UnloadCritter` сначала убирает его с карты, поэтому тесты выгрузки и загрузки перед выгрузкой вызывают `cr.MakePersistent(true)` для самого персонажа.
- `Game.LoginPlayerToNewRecord` отклоняет нового игрока, запись которого оказалась бы пустой, поэтому вход в песочницу перед входом задаёт одно сохраняемое свойство `Player` — `SandboxAccount`.
- Исходные файлы тестов запекаются вместе с рецептами, потому что эта песочница ничего не поставляет. Игровой проект не допускает их в поставляемые паки; см. [Тесты в игровом проекте](../../Docs/ru/how-to/project/project-tests.md).
- `Scripts/Testing.fos` должен оставаться побайтно идентичным `Examples/GameplayTestHarness/ScriptTests/AngelScript/Testing.fos`; это проверяет `BuildTools/tests/test_mechanics_sandbox.py`.
- CI Engine выполняет для `Examples/` только статические проверки. Полный маршрут, описанный выше, требует нативной сборки и запускается по требованию.
