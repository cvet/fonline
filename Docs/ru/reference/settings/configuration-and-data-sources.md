---
layout: default
title: Конфигурация и источники данных
locale: ru
document_id: configuration-data-sources
permalink: /Docs/ru/reference/settings/configuration-and-data-sources.html
---
<!-- docs-translation: {"document_id":"configuration-data-sources","locale":"ru","source_path":"Docs/en/reference/settings/configuration-and-data-sources.md","source_sha256":"3a756bc7339d85f1185928f1656015e2618be7fc0d03cae7fa8510964d78c405"} -->
# Конфигурация и источники данных

> Документация движка. Эта страница описывает переиспользуемые механизмы разбора конфигурации, runtime settings, смонтированные источники данных, поиск файлов и хранение кэша. Конкретные значения конфигурации и правила размещения контента принадлежат встраивающему проекту.

## Назначение

Используйте эту страницу при изменении того, как движок читает `.fomain` и другие конфигурационные данные, применяет переопределения командной строки или sub-config, монтирует каталоги и пакеты ресурсов, читает файлы либо хранит кэшированные данные ресурсов.

Практический маршрут по настройке исполняемого проекта приведен в разделе [Настройка игрового проекта](../../how-to/build/project-configuration.md). Эта страница остается владельцем переиспользуемой реализации и справочного контракта.

Читайте ее вместе со следующими материалами:

- [Процесс сборки](../../how-to/build/) описывает точки входа конфигурации и сборки.
- [Конвейер baking](../../explanation/content-pipeline/baking.md) описывает производство resource packs.
- [Текст и локализация](../../how-to/content/text-and-localization.md) описывает `Baking.BakeLanguages`, `Client.Language`, выбор имени text pack и нормализацию при baking.
- [Форматы шрифтов и компоновка текста](../../how-to/content/font-format.md) описывает raw-copy для font descriptor, поиск связанных изображений, client binding и принадлежащую проекту политику slots.
- [Сгенерированный API и метаданные](../metadata/index.md) описывает сгенерированные settings и входы metadata.
- [Клиентский runtime](../../explanation/runtime/client.md), [серверный runtime](../../explanation/runtime/server.md) и [инструменты](../tools/) описывают runtime и tool consumers.

## Проверенные пути исходников

- `Source/Common/ConfigFile.h`
- `Source/Common/ConfigFile.cpp`
- `Source/Common/Settings.h`
- `Source/Common/Settings.cpp`
- `Source/Common/Settings.inc`
- `Source/Common/DataSource.h`
- `Source/Common/DataSource.cpp`
- `Source/Common/FileSystem.h`
- `Source/Common/FileSystem.cpp`
- `Source/Common/CacheStorage.h`
- `Source/Common/CacheStorage.cpp`
- `Source/Common/SettingsStorage.h`
- `Source/Common/SettingsStorage.cpp`
- `Source/Essentials/DiskFileSystem.h`
- `Source/Essentials/DiskFileSystem.cpp`
- `Source/Essentials/Platform.h`
- `Source/Essentials/Platform.cpp`
- `Source/Frontend/ApplicationInit.cpp`
- `Source/Client/Client.cpp`
- `Source/Client/Updater.cpp`
- `Source/Client/ResourceManager.h`
- `Source/Client/ResourceManager.cpp`
- `Source/Tools/Baker.h`
- `Source/Tools/Baker.cpp`
- `Source/Tools/ConfigBaker.h`
- `Source/Tools/ConfigBaker.cpp`
- `BuildTools/cmake/stages/Codegen.cmake`
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake`
- `BuildTools/cmake/stages/Packages.cmake`
- связанные тесты в `Source/Tests/`

## Карта слоев

1. **Парсер текста конфигурации** - `ConfigFile` разбирает sections, keys, values, повторяющиеся sections, опционально собираемое содержимое и чтение первой section.
2. **Модель settings** - `Settings.inc` объявляет группы settings; `Settings.*` преобразует config files, переопределения командной строки, internal config, defaults, auto-settings, sub-configs и объявления resource packs в `GlobalSettings`.
3. **Абстракция источника данных** - `DataSource` монтирует disk directories и pack files за единым интерфейсом перечисления и открытия файлов.
4. **Представление файловой системы** - `FileSystem` объединяет смонтированные источники данных, предоставляет `FileHeader`, `File`, `FileReader` и `FileCollection`, а также разрешает чтение по пути или имени.
5. **Хранилище кэша** - `CacheStorage` сохраняет именованные string/data entries для переиспользуемых потребителей кэша.
6. **Хранилище settings** - `SettingsStorage` сохраняет пользовательские настройки инструментов и редакторов (registry в Windows, файловое хранилище на остальных платформах) в области имени приложения.
7. **Низкоуровневый доступ к диску** - `DiskFileSystem` выполняет прямые операции с диском ниже слоя смонтированных ресурсов движка.

## Разбор конфигурации

`Source/Common/ConfigFile.*` владеет синтаксическим разбором. `ConfigFileOption` управляет дополнительным поведением:

- `CollectContent` сохраняет содержимое section для потребителей, которым нужен исходный текст блока.
- `SkipNestedSections` разбирает только anchor sections и пропускает тела вложенных sections с адресацией через `/`. Это дешевый способ перечислить заголовки в файлах с большим вложенным payload, например в map files.
- Section считается вложенной, если ее имя содержит `/`. `ConfigFile` распознает только синтаксис: имена хранятся **дословно**, а prefix никогда не разрешается автоматически. Семантика prefix принадлежит формату-потребителю. `GetOrderedSections()` возвращает sections в порядке файла, что позволяет связать вложенную section с предшествующим anchor. Multimap по имени этого выразить не может, потому что повторяющиеся имена схлопываются. `SkipNestedSections` разбирает только невложенные sections и пропускает тела вложенных.
- `ConfigFile` получает только содержимое: без identity файла, parse callbacks и format tokens. Для map files интерпретацией владеет `MapLoader`: `[ProtoMap]` объявляет map с именем из `$Name` или из имени файла, а вложенный prefix `$Name/<Type>` связывает содержимое с предыдущим anchor.

Пробел/табуляция и конечный обратный слеш продолжают строку. Накопленное
продолжение у EOF разбирается одинаково с завершающим переводом строки и без него.
Пустые значения допустимы; ограничения формата проверяет потребитель.

Парсер хранит собственные строки и возвращает `string_view` из разобранных sections. Потребитель не должен считать, что эти views живут дольше экземпляра `ConfigFile`.

## Runtime settings

Защищённый сетевой канал добавляет `ServerNetwork.ChannelSecretKey` (обязательный статический секрет сервера) и `ClientNetwork.ChannelServerKeys` (публичные ключи, закреплённые клиентом). Это настройки запуска, а не изменяемое состояние скриптов. Серверная настройка содержит сам ключ из 64 шестнадцатеричных знаков, не путь: `$TARGET_FILE{...}` читает защищённый файл на целевом хосте без встраивания его содержимого при bake. Пустой или неверный серверный ключ блокирует запуск даже interthread-only сервера; отсутствие допустимого pin блокирует подключение клиента. См. [Сеть](../../explanation/authority-and-networking/#защищённый-канал) и [Безопасность и секреты](../../how-to/release/security-and-secrets.md).

`Source/Common/Settings.inc` является центральным декларативным файлом для групп и отдельных settings. Каждый built-in setting адресуется полным именем `Group.Name`, меняется только при построении startup snapshot в `GlobalSettings` и затем доступен как `const`. Live state принадлежит своему runtime owner, а не записывается обратно в settings. `Settings.h` предоставляет:

- `ResourcePackInfo` - имя, входные каталоги и файлы, include/exclude glob patterns, side flags и список bakers;
- `SubConfigInfo` - именованные config overlays и карты settings;
- `GlobalSettings` - объединенные client/server/baking/base settings с операциями применения, сохранения и работы с custom settings.

`GlobalSettings` применяет входные данные через:

- `ApplyConfigAtPath()` и `ApplyConfigFile()` для config files;
- `ApplyCommandLine()` для runtime и build-tool overrides;
- `ApplyInternalConfig()` для сгенерированного internal config;
- `ApplySubConfigSection()` для именованных overlays;
- `ApplyDefaultSettings()` и `ApplyAutoSettings()` для defaults движка и производных значений.

При обычном запуске приложения создается non-baking `GlobalSettings`, а defaults движка применяются до чтения входов проекта. Итоговый runtime order таков: defaults, конфигурация проекта или упакованный internal config, выбранные sub-configs, writable local-config cache, переопределения командной строки, затем производные auto settings. Поэтому проектный `.fomain` фиксирует намеренно заданные значения, а пропущенный setting получает объявленный Engine default, а не нулевое инициализированное значение. Явно заданный ноль остаётся override. Тест `ApplicationSettings` в `Source/Tests/Test_Settings.cpp` проверяет обычный путь `LoadAppSettings()` на изолированных файлах конфигурации и кэша: default baseline и каждый слой override вплоть до командной строки.

`ConfigBaker` из `Source/Tools/ConfigBaker.cpp` заново выводит каждый sub-config
из root. Metadata хранит настроенное root-значение каждого game setting и служит
runtime baseline: `BaseEngine` применяет его, только если config, sub-config,
local config или command line не задавали это имя. Поэтому side-specific internal
config содержит лишь sub-config deltas game settings; false и empty deltas тоже
записываются, поскольку обязаны перекрыть непустой metadata baseline. Built-in
server/client settings сохраняют прежние правила компактной записи непустых
значений. `MetadataBaker` включает timestamps применённых configs в freshness и
отклоняет game-setting declaration без настроенного значения.

Этот metadata baseline применяется только после создания `BaseEngine`.
Game setting, который читает `ApplicationInitHook` или более ранний startup
path, поэтому обязан находиться в самом binary config.
`Baking.BootstrapGameSettings` перечисляет только такие исключения:
`ConfigBaker` записывает каждый указанный setting полностью для каждого baked
sub-config, не сокращая его до delta. Каждое имя обязано разрешаться в
объявленный game setting, иначе baking завершается ошибкой. Список должен
оставаться узким, чтобы фиксированная internal-config patch area размером
20000 bytes содержала bootstrap data, а не вторую копию metadata baseline.

`GlobalSettings::Save()` по-прежнему выводит только settings из
`_appliedSettings`, который заполняется keys применённых configs и baking-mode
allow-list **auto-settings**. Runtime-only settings (platform/build flags, размер
монитора, command-line/git/compatibility values и разрешённый `Common.UserWritablePath`)
должны оставаться в этом allow-list. Settings, используемые только
`BuildTools/package.py`, проверяются как обычные settings. Built-in lookup
принимает только dotted-форму `Group.Name`; старое unqualified имя становится
отдельным custom setting и не меняет built-in value. Script
`SetRuntimeSetting` также отклоняет запись built-in settings как read-only, но
остаётся доступен для project-owned custom settings.

Managed getters numeric, boolean и enum settings используют typed indexed ABI. Built-in entry читает immutable field `GlobalSettings`, а project entry `///@ Setting` — custom map. Custom value один раз разбирается в typed cell записи ABI и переиспользуется, пока не изменится `GlobalSettings::GetCustomSettingsGeneration()`. Каждый writer custom map (`SetRuntimeSetting`, `SetCustomSetting`, `SetValue`, `CopyFrom`) увеличивает generation. String/list settings сохраняют name-based helpers; оба пути после hash dispatch проверяют полное имя setting, поэтому коллизия hash custom setting не может затенить built-in.

Оба скриптовых бэкенда предоставляют вложенные пути: `Android.GradleDependency.Sample` доступен как `Settings.Android.GradleDependency.Sample`. Полный dotted runtime-ключ сохраняется. Оба бэкенда используют `SplitSettingPath()` из `Source/Common/Settings.*` для разбиения канонических путей и отказа пустых сегментов; объявления классов/handles принадлежат языковым генераторам. Built-in настройки в обоих бэкендах read-only; custom настройки AngelScript сохраняют прежние getters/setters, а Managed properties имеют только getter. Внутренние группы AngelScript кодируют длины сегментов, поэтому `SettingsPath.A_B.Value` и `SettingsPath.A.B.Value` остаются разными. Пустые сегменты и конфликты имён property/group дают ошибку при регистрации/bake. Используйте прямой доступ через `Settings`, не указывая сгенерированные типы `GlobalSettingsGroup_*`; после смены их идентичности согласованно пересоберите native-компоненты и запеките все роли AngelScript.

У custom settings есть две формы чтения. Используйте `FindCustomSetting()`, когда отсутствие ключа нормально и должно оставаться в nullable pointer vocabulary. Используйте `GetCustomSetting()` только для совместимости с историческим non-null sentinel behavior: при наличии ключа он возвращает сохраненное значение, иначе `_emptySetting`.

Не документируйте содержимое `.fomain` одного встраивающего проекта как универсальное поведение движка. Конкретные значения описываются в документации проекта, а механика их потребления движком - на этой странице.

`Baking.BakeLanguages` является упорядоченным контрактом контента, а не неупорядоченным locale allowlist: text baking использует первое значение как базу нормализации. `Client.Language` выбирает начальный client text pack. Точное поведение `.fotxt`, `$Text`, fallback и runtime lookup описано в разделе [Текст и локализация](../../how-to/content/text-and-localization.md).

До открытия log, config и local-config cache выполняется отдельный startup-шаг. `ResolveWritableRoot(args)` в `Source/Frontend/ApplicationInit.cpp` определяет read-only `Common.UserWritablePath`, не обращаясь к settings: первым приоритетом служит `--Common.UserWritablePath <path>`, marker `INSTALLED` рядом с executable выбирает per-OS user-data directory плюс `FO_NICE_NAME`, а при отсутствии обоих пустое значение сохраняет portable layout относительно working directory. Специальное CLI-значение `*` запрашивает тот же per-user directory, что и marker. На Android base directory берётся из SDL internal storage, на остальных платформах — через `Platform::GetUserDataBase()`. Config file не может задавать этот путь, потому что сам config может находиться под ним. Разрешённое значение применяется к settings snapshot до чтения local cache, затем создаются обязательные cache/resource subdirectories; ошибка записывает warning и безопасно возвращает процесс к working directory.

Любое непустое `Common.UserWritablePath` в основном config, sub-config,
local-config cache или упакованном internal config вызывает `SettingsException`
с сообщением `Writable root is resolved before settings`, в том числе при baking.
Удалите такое присваивание и передайте нужный root через startup command line.
Пустое сгенерированное присваивание ничего не меняет и сохраняет разрешённое
значение; обычный CLI-pass тоже оставляет его прежним. Проверка выполняется до
подстановок setting и разбора append.

Обычный command line по-прежнему применяется к live settings ровно **один раз**, после config, sub-config и local config, поэтому имеет окончательный приоритет, а `+`-append overrides (`-Setting +value`) не накапливаются дважды. Этот проход журналирует каждое переопределение как `Set <name> to <value>`. Settings, чьи имена содержат token из `Common.SecretSettingTokens` (регистронезависимые подстроки, default `secret token password apikey`), журналируются как `Set <name> to ***`. Это не общая защита credentials: raw process arguments и значения settings остаются доступными, а другие logs, settings UI, crash output, baked configs и project code имеют собственные пути утечки. Не передавайте credentials через command line; используйте provisioning для целевого окружения и следуйте разделу [Безопасность и секреты](../../how-to/release/security-and-secrets.md).

`CommandLineArgs::IsOption()` считает аргумент опцией, только если после начального дефиса идёт буква. Поэтому `--Render.Sleep -1` и другие отрицательные значения не превращаются в новые опции. Строковая настройка сохраняет текст буквально, включая обратные косые черты Windows и кавычки; список строк делится только по пробелам и табуляции. Числовые, логические и enum-настройки по-прежнему разбирают текст. Грамматика экранированных строк `AnyData` применяется к свойствам, но не к настройкам (`Source/Common/Settings.cpp`, `Source/Tests/Test_Settings.cpp`).

## Resource packs и источники данных

`ResourcePackInfo` описывает входы resource pack, используемые bakers и runtimes. На стороне baking применяются `BakingContext` из `Source/Tools/BakingContext.*` и `BakerDataSource` из `Source/Tools/Baker.*`; на стороне runtime применяются смонтированные абстракции `DataSource` и `FileSystem`.

Входные каталоги resource pack монтируются рекурсивно. `IncludePatterns` и `ExcludePatterns` являются необязательными списками glob patterns, разделенными пробелами; они применяются к нормализованным resource-relative paths до запуска любого baker. Пустой include list принимает любой path; exclusion проверяется после inclusion и имеет приоритет. Patterns чувствительны к регистру и поддерживают:

- `*` - ноль или больше символов, кроме `/`;
- `?` - ровно один символ, кроме `/`;
- `**` - ноль или больше символов, включая `/`; `**/` также соответствует нулю уровней каталогов.

В качестве разделителей pattern принимаются и `/`, и `\`, после чего они нормализуются в `/`. Например, `IncludePatterns = **/*.fomap` выбирает maps на любой глубине, а `ExcludePatterns = **/_*.fomap` исключает scratch maps вроде `Generated/_compose.fomap`. Несколько packs могут монтировать одни и те же `InputDirs`, выбирая непересекающиеся ресурсы разными списками patterns. `IncludePatterns = *` воспроизводит прежнее поведение с входами только верхнего уровня.

`DataSource` предоставляет две встроенные формы mount:

- `MountDir(dir, recursive, non_cached, maybe_not_available)` для ресурсов disk directory;
- `MountPack(dir, name, maybe_not_available)` для упакованных resource data.

`MountPack` ищет сначала `.fores`, затем ZIP-совместимые `.zip`/`.bos`, затем Fallout `.dat`. Packaging пишет по одному `<Name>.fores` на логический pack; ZIP сохраняется только для Embedded внутри executable. Полная база `.fores` может иметь один writable `.patch.fores` с полным актуальным каталогом — это не каталог переопределений и не цепочка патчей. Повреждённый `.fores` не подменяется старым одноимённым ZIP. Бинарный формат, хеши, восстановление и кэш `.foindex` описаны в [формате пакетов ресурсов](../../../ResourcePackFormat.md).

Arcanum DAT 2.1 хранит 24-байтную запись плюс имя. Имя может включать завершающий нуль C-строки; reader удаляет завершающие нули, нормализует слеши и переводит пути поиска в нижний регистр. Сжатие (`0x2`), каталог (`0x400`) и игнорирование (`0x800`) — битовые флаги; каталоги и игнорируемые записи исключаются. Усечённые заголовки дерева, записи и диапазоны имени отклоняются при монтировании. Корректные записи Fallout DAT 2.0 сохраняют прежнее поведение; неполные записи и неверные границы дерева отклоняются. После обновления reader пересоберите baker и перезапеките пакеты, импортированные из DAT 2.1.

Затем `FileSystem` объединяет источники и предоставляет:

- `AddDirSource()`, `AddPackSource()`, `AddPacksSource()` и `AddCustomSource()`;
- `FilterFiles()`, `GetAllFiles()` и existence checks;
- `ReadFile()`, `ReadFileText()` и `ReadFileHeader()`;
- helpers `FileReader` для endian-aware binary reads.

Cached directory mounts создают snapshot файлового индекса при монтировании. Долго работающие инструменты могут вызвать `FileSystem::ReindexDataSources()`, чтобы каждый смонтированный источник обновил snapshot; метод возвращает `true`, если изменились indexed paths, sizes или write times. Источники без кэширования disk state сохраняют default no-op behavior. Custom sources могут переопределить `DataSource::Reindex()`; `BakerDataSource` использует это для перестроения input mounts и baking новых или измененных ресурсов по запросу.

Порядок mount влияет на lookup behavior. При его изменении проверяйте runtime/tool path, владеющий resource pack, а не только parser.

### Общий индекс смонтированных источников

Точечные lookup-операции (`IsFileExists()`, `ReadFile()` и `ReadFileHeader()`) используют один общий индекс, когда каждый смонтированный источник предоставляет полный `DataSource::GetIndexSnapshot()`. Snapshot предоставляют `ResourcePackSource`, `ZipFile`, `EmbeddedFile` и `FalloutDat`; пустой `DummySpace` для отсутствующего необязательного pack возвращает пустой snapshot. Directory sources, включая `CachedDir`, сохраняют default `nullopt`, поэтому добавление любого directory или другого live source отключает индекс для этого `FileSystem` и сохраняет ordered probing. Файловые системы packaged runtime, состоящие только из packs, индексируют ресурсы, а unpacked development mounts, входные каталоги baker-а и смешанные файловые системы updater-а сохраняют live lookup behavior.

Snapshot нового источника снимается до его публикации. Более поздний mount имеет больший приоритет и заменяет каждый заявленный им indexed path, что совпадает с существующим reverse-mount probe order. `ReindexDataSources()` строит новый индекс отдельно и заменяет прежний только после успешного получения всех snapshots; `CleanDataSources()` очищает и источники, и индекс. После setup точечное чтение остаётся lock-free. Если indexed source владеет путём, но не может открыть его, `ReadFile()` возвращает отсутствие файла, не переходя к lower-priority duplicate.

`FilterFiles()` и `GetAllFiles()` продолжают перебирать источники по порядку даже при индексированных точечных lookup-операциях. Загрузка script modules, регистрация прототипов и другие consumers зависят от source-by-source order, который unordered index не сохраняет.

`Common.Packaged` — fixed auto-setting, заполняемый из packaged marker исполняемого файла в `GlobalSettings::ApplyAutoSettings()`. После загрузки настроек runtime-политика обязана читать этот снимок (`settings.Packaged`), чтобы скопированные или подставленные настройки оставались внутренне согласованными и тестируемыми. Прямые проверки `IsPackaged()` зарезервированы за решениями bootstrap до настроек и за `FileSystem::AddPackSource()`, где физический marker исполняемого файла намеренно выбирает монтирование архива или каталога; тесты также могут смотреть на этот marker при выборе совместимых фикстур.

Упакованный клиент выбирает один effective source на каждый логический pack: writable замену `Pack.fores`, если она есть, иначе установленную базу, плюс writable `Pack.patch.fores`, привязанный к физическому хешу этой базы. Полный каталог пары определяет как присутствующие, так и удалённые пути; порядок приоритета логических packs сохраняется. `GetClientResources()` собирает одинаковый вид для gameplay и проверки metadata в updater. Для суффикса после последнего Embedded может применяться удаляемый кэш `Resources.foindex`: устаревший или повреждённый кэш заменяется прямым mount авторитетных пар, а повреждение самих packs остаётся ошибкой. При абсолютном installed/APK root замены хранятся в `<UserWritablePath>/Resources`. См. [формат пакетов ресурсов](../../../ResourcePackFormat.md) и [разделение client runtime и updater](../../explanation/runtime/client-updater.md).

`ResourceIndexSource` и `ResourcePackSource` передают decoded byte vector во владение file buffer holder. Allocation сохраняется до освобождения `File` вызывающей стороной: передача большого stored resource больше не создаёт вторую полноразмерную копию. Во время decompression deflated entries по-прежнему одновременно удерживают compressed и decoded buffers. Это уменьшение пика памяти, а не streaming или новый формат pack. `FileBufferHolderMovesVectorStorage` в `Test_ResourceIndex.cpp` проверяет сохранение указателя и пустые buffers.

## Низкоуровневый доступ к диску

`Source/Essentials/DiskFileSystem.*` выполняет прямые операции с диском ниже смонтированных ресурсов Engine. `fs_write_file()` записывает содержимое по запрошенному пути, но на файловой системе без учёта регистра существующая entry, отличающаяся только регистром букв, переиспользуется со старым написанием. Callers, перезаписывающие принадлежащее им дерево с точными именами, должны явно согласовать такие entries. Baker делает это один раз за проход; см. [согласование имён output](../../explanation/content-pipeline/baking.md#имена-output-согласуются-с-именами-к-которым-обращались-baker-ы).

## Хранилище кэша

`Source/Common/CacheStorage.*` хранит именованные binary/string cache entries через `HasEntry()`, `GetString()`, `GetData()`, `SetString()`, `SetData()` и `RemoveEntry()`. Потребители с жёстким пределом используют `GetDataBounded(name, max_size)`: метод проверяет размер файла до выделения памяти и различает результаты `Success`, `Missing`, `TooLarge` и `Failed`. `SetDataChecked(...)` сообщает, удалось ли записать данные полностью. Низкоуровневый helper `fs_read_file_bounded()` применяет тот же предел до allocation и не читает файл, если он слишком велик. Хранилище отделено от resource packs: cache entries являются изменяемыми runtime/tool artifacts, а baked resources генерируются из настроенных inputs. Client-side cache consumers разрешают относительные cache paths через `fs_make_writable_path(UserWritablePath, CacheResources)`, поэтому portable clients хранят cache рядом с executable, а installed clients пишут под per-user root.

Файловый backend публикует строковые и бинарные entries через `fs::write_file`. Уже открытые читатели сохраняют прежнее полное значение; последующие открытия получают новое. При ошибке публикации запись сохраняется, ошибка журналируется, а `SetDataChecked` возвращает `false`. Инвалидация остаётся явной операцией `RemoveEntry`; неудачная проверяемая запись не означает, что обновление было сохранено.

Каждый entry хранится как один обычный файл с именем entry, в котором path separators заменены на `_`; поэтому cache directory остается читаемым и пригодным для проверки. Два имени entry, отличающиеся только такими separators, указывают на один файл. Для кэша это допустимо, поскольку miss всегда восстанавливаем, но caller, которому нужны разные entries, не должен полагаться только на структуру каталогов для их разделения. Cache не является confidentiality boundary: все, что нельзя хранить открыто, должно быть защищено владельцем до передачи в хранилище. Именно так работает secure-storage bridge встраивающего проекта.

## Хранилище settings

`Source/Common/SettingsStorage.*` сохраняет небольшие пользовательские настройки инструментов и редакторов (ImGui window layout, view options, last selection) через `GetString()`/`SetString()`, типизированные `GetInt`/`SetInt`, `GetBool`/`SetBool`, `GetFloat`/`SetFloat`, а также `HasKey()` и `Remove()`. Область задается именем приложения в конструкторе, поэтому разные инструменты не конфликтуют. Platform-specific backend скрыт за pimpl: при `FO_WINDOWS` значения являются `REG_SZ` entries в `HKCU\Software\FOnline\<app_name>` (Win32 headers остаются в `.cpp` за `WIN32_LEAN_AND_MEAN` и `WinApiUndef.inc`, используются явные registry entry points `*A`); на остальных платформах применяется per-application `CacheStorage` в `Platform::GetUserDataBase()/FOnline/<app_name>`. Каждое значение хранится строкой, а typed accessors сериализуют через нее, поэтому оба backends ведут себя одинаково и многострочный ImGui blob `imgui.ini` проходит round trip без изменений. Persistence работает по принципу **best effort**: backend failure журналируется, но не выбрасывается, поэтому инструмент не завершается из-за невозможности записать settings. От `CacheStorage` этот слой отличается назначением (долговечные user preferences вместо восстанавливаемых cache artifacts), а в Windows еще и носителем (registry вместо файлов).

Его используют только GUI tools: Mapper через `MapperEngine::_uiSettings`, перенесенный из resource `Cache`, а также standalone AnimationViewer и ParticleViewer, загружающие settings в конструкторе и сохраняющие при shutdown. Код находится в `CommonLib` для простоты, но client и server не ссылаются на символы `SettingsStorage`, поэтому linker (`/OPT:REF` и on-demand inclusion static library) исключает объект из поставляемых client/server binaries. Windows registry calls тем самым не попадают в них и не вызывают лишних antivirus heuristics. Собственный autosave ImGui `imgui.ini` отключен в `Application.cpp`, поэтому вся persistence layout проходит через это хранилище.

## Маршрутизация сборки и package

- `BuildTools/cmake/stages/Codegen.cmake` генерирует входы internal config, используемые runtime settings.
- `BuildTools/cmake/stages/ScriptsAndBaking.cmake` связывает resource baking и script compilation, потребляющие `ResourcePackInfo` и baking settings.
- `BuildTools/cmake/stages/Packages.cmake` упаковывает ресурсы для runtime targets.
- `Source/Tools/ConfigBaker.*` выпекает config resources; полная оркестрация описана в [конвейере baking](../../explanation/content-pipeline/baking.md).

## Проверяемые тесты

Сфокусированные тесты этой области:

- `Source/Tests/Test_CacheStorage.cpp`
- `Source/Tests/Test_SettingsStorage.cpp`
- `Source/Tests/Test_ConfigFile.cpp`
- `Source/Tests/Test_DataSource.cpp`
- `Source/Tests/Test_DiskFileSystem.cpp`
- `Source/Tests/Test_FileSystem.cpp`
- `Source/Tests/Test_Settings.cpp`
- `Source/Tests/Test_ConfigBaker.cpp`

Связанные consumers покрываются resource, client, server, script и baker tests, перечисленными в разделе [Тестирование](../../contributing/testing/).

## Маршрутизация изменений

- Config grammar и поведение разобранных sections/keys: `Source/Common/ConfigFile.*`.
- Группы settings, defaults, применение command-line/config/sub-config: `Source/Common/Settings.*` и `Settings.inc`.
- Разрешение writable root установленного клиента: `Source/Frontend/ApplicationInit.cpp`, `Source/Essentials/Platform.*` и `Source/Essentials/DiskFileSystem.*`.
- Поиск смонтированных ресурсов: `Source/Common/DataSource.*` и `FileSystem.*`.
- Необработанные disk operations: `Source/Essentials/DiskFileSystem.*`.
- Потребление runtime resources: `Source/Client/ResourceManager.*` и соответствующая runtime documentation.
- Выбор particle source, компиляция `.spark`/`.efkproj` и runtime consumption `.spk`/`.efk`: `Source/Tools/ParticleBaker.*`, `Source/Client/ParticleRuntime.*`, `Source/Client/VisualParticles.*` и [формат и runtime частиц](../../how-to/content/particle-format.md).
- Выбор font descriptor для raw-copy и runtime consumption: `Baking.RawCopyFileExtensions`, `Source/Tools/RawCopyBaker.*`, `Source/Client/FontManager.*` и [форматы шрифтов и компоновка текста](../../how-to/content/font-format.md).
- Audio baking, sound indexing, Vorbis decoding и client playback: `Baking.AudioVorbisQuality`, `Audio.SoundFileExtensions`, `Audio.*`, `Source/Tools/AudioBaker.*`, `Source/Client/AudioManager.*` и [Audio](../../how-to/content/audio.md).
- Video raw-copy, exact-path loading, Ogg/Theora decode, fullscreen/embedded client playback и владение memory: `Baking.RawCopyFileExtensions`, `Source/Tools/RawCopyBaker.*`, `Source/Client/VideoClip.*`, `Source/Client/Client.*` и [Video](../../how-to/content/video.md).
- Генерация resource packs: [Конвейер baking](../../explanation/content-pipeline/baking.md) и `Source/Tools/*Baker.*`.

## Контрольный список проверки

1. Запустите сфокусированные parser/settings/filesystem/cache tests для измененной области.
2. Если изменилась форма resource pack или порядок mount, выполните хотя бы один bake path и один runtime/tool consumer path.
3. Если изменилось поведение command line или sub-config, проверьте конфигурацию встраивающего проекта, которая его использует, но оставьте project-specific values в документации проекта.
4. Если изменился packaging или resource staging, при необходимости повторно проверьте [Web build, packaging и browser debugging](../../how-to/platforms/web-debugging.md), [Android build, packaging и device debugging](../../how-to/platforms/android-debugging.md) и [разделение client runtime и updater](../../explanation/runtime/client-updater.md).
5. При изменении владения build stages обновите [конвейер baking](../../explanation/content-pipeline/baking.md) или [конвейер BuildTools](../cmake-and-buildtools/pipeline.md).

## Ошибки загрузки ресурсов

Отсутствующий результат поиска остаётся `false`/`nullptr`. Повреждённый смонтированный источник выдаёт `DataSourceException`: ошибки дерева DAT содержат архив, формат, поле и нужные размеры/смещение записи; ошибки чтения — архив и путь ресурса, ошибки zlib — код возврата. DAT 1.0 явно не поддерживается. Распаковка DAT должна достичь конца потока и совпасть с заявленным размером, включая пустые записи.

Ошибки ZIP/embedded ZIP содержат пакет, запись либо индекс и код minizip. Неполное чтение и ошибка закрытия/CRC различаются. Имя с терминатором должно помещаться в буфер 4096 байт, размер распакованной записи — в знаковое 32-битное поле. Неполное чтение cached-directory выбрасывает исключение; после изменения файлов обновляйте индекс. Ошибка границ `FileReader` содержит позицию, число запрошенных байтов и размер буфера, не сдвигая курсор. Загрузчики передают ошибку выше; журналирование принадлежит границе обработки.

## Опциональные провайдеры архивов

`DataSource.cpp` владеет dispatch; соседние пары `DataSource-Directory`, `DataSource-Embedded` обязательны, `DataSource-Dat` требует `FO_DATA_SOURCE_DAT`, `DataSource-Zip` — `FO_DATA_SOURCE_ZIP`. Регистрация/includes безусловны, guards внутренние. [Выбор компонентов](../../how-to/build/project-configuration.md#optional-native-components) описывает роли/зависимости.

Приоритет: Embedded / `.fores` / `.zip` / `.bos` / `.dat`. Существующий выключенный архив сообщает путь/опцию даже при `maybe_not_available=true`; пустой источник — только для отсутствующего optional pack. Повреждённый приоритетный pack не подменяется следующим. Embedded/APK сохраняют bundled minizip при внешнем ZIP OFF; `.fores`/updater patches остаются базовыми.
