---
layout: default
title: Сохранение данных
locale: ru
document_id: persistence
permalink: /Docs/ru/explanation/persistence/
---

# Сохранение данных
<!-- docs-translation: {"document_id":"persistence","locale":"ru","source_path":"Docs/en/explanation/persistence/index.md","source_sha256":"fbb0fc412c265bc1326492a21d1a791142468d154f5eb921f7dce6beee5d2079"} -->
Этот документ описывает серверную абстракцию базы данных, модель коллекций и ключей, очередь commit, согласованные с backend снимки, журналы восстановления и реализации backend.

Используйте его при изменении `Source/Server/DataBase.*`, настроек базы данных, кода загрузки и сохранения сущностей или тестов persistence.

`DataBase-Json.cpp` публикует готовые добавленные и обновлённые JSON records через `fs::write_file`. Уникальный временный файл рядом с записью заменяет прежнюю последовательность с фиксированным `<record>.tmp`. Ключи, JSON-содержимое и семантика commit queue сохраняются. Восстановление снимка SQLite использует тот же атомарный helper для неопубликованной scratch-базы до открытия для восстановления.

## Модель владения

Движок владеет переиспользуемой абстракцией базы данных, реализациями backend и подтверждаемыми исходным кодом границами восстановления. [Backup and Recovery](../../how-to/release/backup-and-recovery.md) определяет нейтральную к провайдеру процедуру резервного копирования и восстановления. Подключающий проект владеет выбором развёртывания, строками подключения, инструментами провайдера, расписаниями, хранением, RPO/RTO, политикой миграции данных и производственными полномочиями.

Не помещайте в этот документ действующие учётные данные, производственные строки подключения или зависящие от хоста шаги восстановления.

## Штатная остановка и владение сущностями

Постоянные свойства держателей описывают сохраняемое владение. `ServerEngine::Shutdown` обязан сохранять эти ID и документы custom entities. `EntityManager::DestroyAllEntities` очищает внутренние ссылки runtime, пока реестр удерживает все сущности, затем сбрасывает родителей и владение реестра. Обычный `DestroyCustomEntity` отдельно изменяет ID держателей и удаляет документы. Различие относится и к деревьям под Game, и к custom children стандартных держателей. `ServerShutdownPreservesPersistentCustomEntityTreesAcrossRestarts` проверяет границу через две реальные перезагрузки файлового хранилища; чтение живого мира до остановки не доказывает долговечность.

## Открытый фасад базы данных

`Source/Server/DataBase.h` определяет открытый фасад `DataBase`. Он оборачивает backend `DataBaseImpl` и предоставляет операции коллекций и документов:

- состояние и метрики: `InValidState()`, `GetDbRequestsPerMinute()`;
- перечисление: `GetAllIds()`, `GetAllIntIds()`, `GetAllStringIds()`;
- чтение: `Get()`, `GetMany()`, `Valid()`;
- запись: `Insert()`, `TryInsert()`, `Update()`, `CompareExchange()`, `Delete()`;
- управление commit: `StartCommitChanges()`, `WaitCommitChanges()`, `ClearChanges()`;
- снимок backend: `CreateSnapshot()` и `RestoreSnapshot(bytes)`;
- отладочный интерфейс: `DrawGui()`.

`ConnectToDataBase()` создаёт фасад из настроек, данных подключения, схем коллекций и panic callback. JSON и `DbSQLite` используют весь остаток строки после имени провайдера, убирая внешние пробелы, как каталог хранилища: `JSON C:/Game Data/world` и `DbSQLite C:/Game Data/world` сохраняют внутренние пробелы. Не добавляйте к этому остатку буквальные кавычки. Пустой каталог отклоняется; Memory и Mongo сохраняют грамматику токенов.

`GetMany(collection, ids)` читает несколько записей одной коллекции одним backend-вызовом `GetRecords()` и возвращает документы в порядке запрошенных ids: отсутствующей записи соответствует пустой документ, повторному id — тот же результат. Каждый документ следует контракту `Get()` с наложенными pending commit operations; отдельно перечитывается только запись, commit которой завершился во время batch read. Наложение прекращается после записи в backend и удаления операции из очереди. Если это произошло после чтения старого значения из backend, но до проверки очереди, соответствующая запись читается повторно; остальные не затрагиваются. Сам `Get()` использует тот же путь для одного id.

Активные читатели разделяют поколение записи, а каждый backend-запрос запоминает собственное ожидаемое поколение. Commit увеличивает его под той же блокировкой, что удаляет pending operation. Другой читатель не может подменить свидетельство commit, сделавшего прежнее чтение устаревшим. Учёт удаляется только при уходе последнего читателя, поэтому исторические завершённые записи не накапливаются. `DataBaseConditionalReadCannotBorrowAnotherReadersCommitMarker` закрепляет порядок двух читателей и commit, а также отказ CAS со старым значением.

`GetAllIds(collection)` и типизированные варианты `GetAllIntIds()` / `GetAllStringIds()` перечисляют ключи backend с наложенными в порядке очереди ожидающими insert и delete, в согласии с тем, что может прочитать `Get()`. Ожидающий delete убирает ключ из списка, ожидающий insert добавляет. Если insert или delete этой коллекции завершается во время перечисления ключей backend, перечисление повторяется. Update не меняет набор ключей. Тесты `DataBaseGetAllDocumentIdsAppliesPendingInsertsAndDeletes` и `DataBaseGetAllDocumentIdsRelistsKeysCommittedDuringListing` проверяют наложение и параллельный commit.

## Условные изменения

`TryInsert(collection, id, document)` возвращает `false`, если логическая запись уже существует. `CompareExchange(collection, id, key, expected, desired)` меняет существующее поле только при совпадении его типизированного значения с `expected`; отсутствие записи, поля или несовпадение возвращают `false` без постановки записи в очередь. Обе операции учитывают pending insert, update и delete при проверке условия. Неверная коллекция/тип ключа, пустой добавляемый документ и не конечное desired-значение по-прежнему вызывают исключение.

Один producer mutex на `DataBaseImpl` объединяет проверку условия с постановкой операции в очередь и сериализует их с обычными insert/update/delete, `RestoreSnapshot()` и `ClearChanges()`. Commit thread использует только существующие блокировки очереди/backend. Результат `true` означает попадание в логическую очередь, а не завершённый durable commit: `StartCommitChanges()` включает обработку, а `WaitCommitChanges()` сохраняет существующие ограничения при отказах и восстановлении. Это не транзакция между экземплярами/процессами и не backend-native compare-and-swap Mongo/SQLite. Для записей с такой гарантией потребитель должен сохранять один владеющий экземпляр.

Серверные скрипты получают `Game.DbTryInsertRecord(collection, id, keyValues)` и `Game.DbCompareExchangeRecordString(collection, id, key, expected, desired)` для целочисленных и строковых ids. Эти методы строковых полей проверяют коллекцию/id, непустые добавляемые значения/ключ и UTF-8 до вызова общего слоя БД. Live entity cover не требуется. Поведение `DbInsertRecord` с исключением при дубликате не меняется.

## Коллекции и ключи

Слой базы данных хранит значения `AnyData::Document` в именованных коллекциях.

`DataBaseImpl` рекурсивно проверяет документы до постановки insert или update в очередь и отклоняет не конечные значения `Float64` во вложенных документах и массивах. Преобразование JSON и BSON применяет то же правило в обоих направлениях, поэтому неверное число останавливается на границе persistence и не попадает в хранилище или runtime-состояние.

Основные типы:

- `DataBaseKeyType` — `IntId` или `String`;
- `DataBaseStringKeyEscaping` — `Raw`, `File` или `Hex`;
- `DataBaseKey` — `variant<ident_t, string>`;
- `DataBaseCollection` — map от `DataBaseKey` к `AnyData::Document`;
- `DataBaseCollectionSchema` — пара имени коллекции и типа ключа;
- `DataBaseCollectionSchemas` — список схем, используемый при инициализации.

`DataBaseImpl::ValidateCollectionKey()` требует согласованности схемы коллекции и ID записи. При добавлении новой постоянной коллекции добавьте схему на уровне сервера или entity manager и проверьте все реализации backend.

## Интерфейс backend

`DataBaseImpl` — базовый класс backend. Реализация обязана предоставить:

- `GetStringKeyEscaping()`;
- `GetAllRecordIds()`;
- `EnsureCollection()`;
- `GetRecord()`;
- `InsertRecord()`;
- `UpdateRecord()`;
- `DeleteRecord()`.

Backend может переопределять:

- `GetRecords()` для batch read. Mongo использует chunks `_id: {$in: [...]}` максимум по 1000 ids с согласованными batch size/limit, SQLite — один `key IN (...)` statement на 1000 ids, Memory/JSON читают batch под одним storage lock. Базовая реализация вызывает `GetRecord()` для каждого id;
- `CreateSnapshotData()` и `RestoreSnapshotData()`, когда backend умеет представить всё своё содержимое байтами;
- `TryReconnect()`;
- `DrawGui()`;
- тестовые hook, например `OnCommitOperationWrittenToOpLog()` и `OnPendingChangesRestored()`.

Фабрики из `DataBase.h`:

Фабрики находятся в соседних парах заголовок/реализация под guards:

- `DataBaseMemory::Create()` в `DataBase-Memory.h`, доступна всегда;
- `DataBaseJson::Create()` в `DataBase-Json.h`, при `FO_DATABASE_JSON`;
- `DataBaseSQLite::Create()` в `DataBase-SQLite.h`, при `FO_DATABASE_SQLITE`;
- `DataBaseMongo::Create()` в `DataBase-Mongo.h`, при `FO_DATABASE_MONGO`.

`DataBase.cpp` владеет dispatch, commit/recovery, ключами и метриками. Все заголовки провайдеров подключены без условий; выключенный провайдер не содержит объявлений и реализации. Нейтральное преобразование JSON/CBOR находится в `DataBaseSerialization.h/.cpp`. См. [выбор компонентов](../../how-to/build/project-configuration.md#optional-native-components).

## Очередь commit

Записи представлены commit-операциями:

- `Insert`
- `Update`
- `Delete`

`DataBaseImpl` ставит ожидающие операции в очередь и обрабатывает их механизмом commit-thread:

- `StartCommitChanges()` планирует или запускает обработку;
- `WaitCommitChanges()` ждёт опустошения commit-thread;
- `ClearChanges()` очищает ожидающее состояние;
- `CommitNextChange()` применяет одну операцию;
- `CommitThreadEntry()` выполняет фоновый цикл.

Открытый фасад `DataBase` передаёт запись этому механизму. Реализации backend должны заниматься только долговечной операцией над записью; общая логика владеет планированием, operation log, panic/retry и метриками.

## Согласованные с backend снимки

`DataBase::CreateSnapshot()` возвращает всё содержимое базы данных байтами, а `RestoreSnapshot(bytes)` кладёт такое содержимое обратно. База данных никогда не называет файл, не создаёт каталог и не решает, где живёт снимок: хранение байтов — дело вызывающего. Общая логика `DataBaseImpl` требует для снятия работающего commit-потока, вычерпывает очередь commit, отвергает отказавший backend и затем блокирует новых производителей `Insert()` / `Update()` / `Delete()` до возврата или исключения из backend. Восстановление требует уже пустой очереди и блокирует производителей так же. Параллельные операции снимка сериализуются. Чтение может продолжаться в пределах собственной блокировки хранилища backend.

Backend без байтового представления бросает `DataBaseException` в обе стороны, поэтому неподдерживающий backend падает громко, а не возвращает пустой снимок. Эти операции не выбирают имена слотов, не публикуют пакеты, не пишут манифесты, не снимают состояние движка и не авторизуют сохранение: такие политики принадлежат встраивающему контроллеру сессии. Вызывающий обязан сначала остановить игровых производителей и материализовать всё точное состояние, которое должно попасть в очередь commit.

`DbSQLite::CreateSnapshotData()` сериализует базу через `sqlite3_serialize()`, удерживая блокировку исходного хранилища, поэтому байты — это точный образ страниц, каким база была бы на диске со свёрнутым содержимым WAL. `RestoreSnapshotData()` загружает эти байты в приватную базу в памяти через `sqlite3_deserialize()` и копирует её в живое хранилище через online-backup API: десериализация прямо в живой handle оторвала бы его от файла. Буфер для `sqlite3_deserialize()` выделяется через `sqlite3_malloc64()` и освобождается при закрытии.

`ServerEngine::CreateSnapshot()` — более высокая композиция состояния движка для стабильного авторитетного мира. Сначала достигается quiescence и возвращаются типизированные блокеры для runtime-контекстов скриптов, отложенных замыканий, событий времени и движения; только готовый мир сбрасывает точные время и id, создаёт байты backend и пишет парный версионированный манифест генератора и совместимости. Узкий контракт `DataBase` это не меняет, атомарной публикацией слота не является. Восстанавливающий строго читает манифест, копирует неизменяемые байты базы в изолированный живой каталог и передаёт состояние в свежий `ServerEngine`; открывать выбранный снимок напрямую как живое записываемое хранилище контрактом не поддерживается.

`DataBaseSnapshotDrainsAndBlocksNewProducers` закрепляет общий барьер commit и производителей. `SQLiteDataBaseCreatesReopenableSnapshotWithoutOverwriting` закрепляет включение ожидающих записей, исключение более поздних изменений источника, независимое переоткрытие при живом источнике, отказ перезаписывать готовый результат и отсутствие файла базы после неудачного назначения.

## Журналы восстановления и panic-политика

`DataBaseImpl::RecoveryLogHandle` владеет файлом operation log:

- `GetPath()`;
- `GetLinesCount()`;
- `GetTextSize()`;
- `GetContent()`;
- `Append()`;
- `Truncate()`.

`DataBaseImpl` может хранить журналы ожидающих и подтверждённых изменений:

- `_pendingChangesLog`
- `_committedChangesLog`

При включённом operation log настройка `DataBase.OpLogPath` должна быть непустой. Настроенный путь принадлежит pending-журналу; путь progress-журнала подтверждённых записей получается заменой `.oplog` на `-committed.oplog`. `InitializeOpLogs()` не проверяет этот суффикс, поэтому сохраняйте обычное имя с `.oplog` и убеждайтесь, что производный путь отличается, вместо использования произвольного имени.

Настройки recovery и panic:

- `_pendingChangesPanicThreshold`
- `_panicShutdownTimeout`
- `_reconnectRetryPeriod`
- `_panicCallback`

Связанные методы:

- `InitializeOpLogs()`;
- `RestorePendingChanges()`;
- `StartPanic()`;
- `TryReconnect()`.

При изменении долговечности commit проверяйте восстановление после неудачной записи и восстановление pending-журнала, а не только успешный путь.

## Особенности backend

- JSON backend: файловое и каталоговое хранилище с escaping строковых ключей, подходящим для путей файловой системы.
- SQLite backend: доступен только при `FO_DATABASE_SQLITE` и только серверу — клиент не связывает встроенную базу. Каждая коллекция является таблицей в едином `Storage.sqlite`, журналируемом в WAL; SQLite выделяет память через систему памяти движка посредством `SQLITE_CONFIG_MALLOC`, а `CreateSnapshot()` возвращает сериализованный образ страниц, а не копию живых файлов.
- Mongo backend: доступен только при `FO_DATABASE_MONGO`; только этот провайдер владеет преобразованием BSON и настройкой allocator в `DataBase-Mongo.cpp`.
- Memory backend: подходит для тестов и недолговечных runtime-путей.

`DocumentToBson()` и `BsonToDocument()` — приватные функции Mongo. JSON использует обычный текст JSON, SQLite — CBOR BLOB через `DataBaseSerialization`. `GetDbKeyType()` сообщает, основан runtime-ключ на integer или string.

<a id="storage-formats-and-upgrade-boundary"></a>
## Форматы хранения и граница обновления

JSON-файлы содержат обычные UTF-8 objects, BLOB записи SQLite — CBOR objects. Сохраняются знаковый Int64 (пределы и числа выше 2^53), конечный Float64, Bool, String, Array, Dict, включая NUL строк/ключей. Null/binary, unsigned overflow, другой корень, испорченный/усечённый вход и байты после CBOR отвергаются. Пределы: 64 МиБ на запись, 64 уровня вложенности. SAX проверяет типы/глубину/объявленные размеры до выделения DOM; валидация предшествует записи.

Используйте пустые JSON/SQLite: для старых EJSON/BSON нет reader/converter. Сохраните старые binaries/storage для отката. Семантика Mongo BSON/Memory и SQLite tables/keys/WAL/snapshots сохраняется; снимкам нужны текущие согласованные metadata/binaries. [Компоненты](../../how-to/build/project-configuration.md#optional-native-components) задают выбор провайдера, [backup/recovery](../../how-to/release/backup-and-recovery.md) — согласованные комплекты. Guarded `Test_DataBase.cpp` проверяет codecs, reopen/update/delete и recovery.

## Связь с состоянием сущности

Persistence хранит документы; состояние сущности попадает в документы через сериализацию свойств и серверное управление сущностями.

Неуспешное восстановление криттера сообщает об ошибке, но не сохраняет списки ID постоянных серверных держателей, сокращённые до частичного графа runtime. Поэтому отсутствующие дочерние документы остаются устранимыми без потери постоянных ссылок. Успешные обработчики pre-load и явные миграции по-прежнему сохраняют свои изменения обычным образом.

Значимые понятия из [модели сущностей](../entity-and-property-model/):

- флаги постоянных свойств;
- исключение временных свойств;
- base и overlay данные свойств;
- `ExplicitlyPersistent`;
- ID и записи пользовательских holder;
- производное от прототипа runtime-состояние.

Не добавляйте в `Entity` или `Properties` предположения конкретной базы данных, если их нельзя поддержать всеми backend и тестами.

## Метрики и диагностика

`GetDbRequestsPerMinute()` сообщает недавнее число запросов к базе через посекундные bucket; batch считается один раз на backend-вызов `GetRecords()`, независимо от числа записей. Ошибки backend и попытки reconnect учитываются в состоянии `DataBaseImpl`.

`DrawGui()` доступен на уровнях фасада и backend для отладки и проверки.

## Тесты для проверки

Подходящие тесты:

- `Source/Tests/Test_DataBase.cpp`;
- тесты управления сущностями, например `Test_LocationAndEntityMgmt.cpp`, когда persistence влияет на сохраняемые сущности;
- зависящие от backend тесты при включённых сборочных параметрах.

Скрытый тест `MongoDataBaseRecoversAfterIsolatedServerRestart` требует `FO_TEST_MONGO_URI` (loopback) и `FO_TEST_MONGO_CONTROL` (изолированный каталог с правом записи). Координатор ждёт `ready`, останавливает свой mongod, пишет `stopped`, ждёт `spilled`, перезапускает этот экземпляр и пишет `restarted`; `complete` подтверждает replay и повторное открытие.

## Маршрутизация изменений

- Открытый фасад и общая логика commit/recovery: `Source/Server/DataBase.h` и `Source/Server/DataBase.cpp`.
- JSON backend: `Source/Server/DataBase-Json.cpp`.
- SQLite backend: `Source/Server/DataBase-SQLite.cpp`.
- BSON allocator и преобразование только Mongo: `Source/Server/DataBase-Mongo.cpp`, `InitializeBsonMemory()`, `DocumentToBson()`, `BsonToDocument()`; Mongo-операции остаются в `Source/Server/DataBase-Mongo.cpp`.
- Memory backend: `Source/Server/DataBase-Memory.cpp`.
- Сериализация сущностей и свойств: [модель сущностей](../entity-and-property-model/) и `PropertiesSerializer.*`.
- Переключатели возможностей сборки: [процесс сборки](../../how-to/build/) и [конвейер BuildTools](../../reference/cmake-and-buildtools/pipeline.md).
- Наборы резервных копий, ограничения oplog, изолированное восстановление и disaster-recovery drills: [Backup and Recovery](../../how-to/release/backup-and-recovery.md). Последовательность остановки и rollback развёртывания: [Release Operations](../../how-to/release/operations.md). Выбор провайдера, расписания, хранение, RPO/RTO, конкретные миграции и производственные полномочия принадлежат проекту.

## Контрольный список проверки

1. Запустите `Source/Tests/Test_DataBase.cpp` или эквивалентную тестовую цель базы данных подключающего проекта.
2. Для каждого затронутого backend проверьте insert, update, delete, get, valid и перечисление ID.
3. При изменении ключей проверьте коллекции с integer и string key.
4. Проверьте опустошение commit queue через `StartCommitChanges()` / `WaitCommitChanges()`.
5. При изменении durability или recovery проверьте восстановление operation log после имитации неудачного commit.
6. Проверьте, что при включённом operation log `DataBase.OpLogPath` непуст и замена `.oplog` на `-committed.oplog` даёт отдельный путь committed-журнала.
7. При изменении снимков докажите, что ожидающие записи включены, более поздние изменения источника исключены, назначение переоткрывается при живом источнике, готовое назначение не перезаписывается, а после неудачи файла базы не остаётся.
8. При изменении семантики постоянного свойства проверьте загрузку и сохранение сущности.
9. Никогда не помещайте производственные учётные данные или действующие строки подключения в документацию или тесты репозитория.
