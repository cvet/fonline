---
layout: default
title: Сохранение данных
locale: ru
document_id: persistence
permalink: /Docs/ru/explanation/persistence/
---

# Сохранение данных

<!-- docs-translation: {"document_id":"persistence","locale":"ru","source_path":"Docs/en/explanation/persistence/index.md","source_sha256":"5ea157746d9ac5839d7c0209c905160bca4b177c830f8c2142a6cee8c187a7fa"} -->

Этот документ описывает серверную абстракцию базы данных, модель коллекций и ключей, очередь commit, согласованные с backend снимки, журналы восстановления и реализации backend.

Используйте его при изменении `Source/Server/DataBase.*`, настроек базы данных, кода загрузки и сохранения сущностей или тестов persistence.

## Модель владения

Движок владеет переиспользуемой абстракцией базы данных, реализациями backend и подтверждаемыми исходным кодом границами восстановления. [Backup and Recovery](../../how-to/release/backup-and-recovery.md) определяет нейтральную к провайдеру процедуру резервного копирования и восстановления. Подключающий проект владеет выбором развёртывания, строками подключения, инструментами провайдера, расписаниями, хранением, RPO/RTO, политикой миграции данных и производственными полномочиями.

Не помещайте в этот документ действующие учётные данные, производственные строки подключения или зависящие от хоста шаги восстановления.

## Открытый фасад базы данных

`Source/Server/DataBase.h` определяет открытый фасад `DataBase`. Он оборачивает backend `DataBaseImpl` и предоставляет операции коллекций и документов:

- состояние и метрики: `InValidState()`, `GetDbRequestsPerMinute()`;
- перечисление: `GetAllIds()`, `GetAllIntIds()`, `GetAllStringIds()`;
- чтение: `Get()`, `Valid()`;
- запись: `Insert()`, `Update()`, `Delete()`;
- управление commit: `StartCommitChanges()`, `WaitCommitChanges()`, `ClearChanges()`;
- снимок backend: `CreateSnapshot()` и `RestoreSnapshot(bytes)`;
- отладочный интерфейс: `DrawGui()`.

`ConnectToDataBase()` создаёт фасад из настроек, данных подключения, схем коллекций и panic callback.

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

- `CreateSnapshotData()` и `RestoreSnapshotData()`, когда backend умеет представить всё своё содержимое байтами;
- `TryReconnect()`;
- `DrawGui()`;
- тестовые hook, например `OnCommitOperationWrittenToOpLog()` и `OnPendingChangesRestored()`.

Фабрики из `DataBase.h`:

- `CreateJsonDataBase()`;
- `CreateSQLiteDataBase()` при `FO_HAVE_SQLITE`;
- `CreateMongoDataBase()` при `FO_HAVE_MONGO`;
- `CreateMemoryDataBase()`.

Файлы реализаций:

- `Source/Server/DataBase-Json.cpp`
- `Source/Server/DataBase-SQLite.cpp`
- `Source/Server/DataBase-Mongo.cpp`
- `Source/Server/DataBase-Memory.cpp`
- общая логика в `Source/Server/DataBase.cpp`

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
- SQLite backend: доступен только при `FO_HAVE_SQLITE` и только серверу — клиент не связывает встроенную базу. Каждая коллекция является таблицей в едином `Storage.sqlite`, журналируемом в WAL; SQLite выделяет память через систему памяти движка посредством `SQLITE_CONFIG_MALLOC`, а `CreateSnapshot()` возвращает сериализованный образ страниц, а не копию живых файлов.
- Mongo backend: доступен только при `FO_HAVE_MONGO`; использует общие с JSON и SQLite преобразование BSON и настройку allocator.
- Memory backend: подходит для тестов и недолговечных runtime-путей.

`DocumentToBson()` и `BsonToDocument()` преобразуют `AnyData::Document` и BSON payload, используемый JSON, SQLite и Mongo. `GetDbKeyType()` сообщает, основан runtime-ключ на integer или string.

## Связь с состоянием сущности

Persistence хранит документы; состояние сущности попадает в документы через сериализацию свойств и серверное управление сущностями.

Значимые понятия из [модели сущностей](../entity-and-property-model/):

- флаги постоянных свойств;
- исключение временных свойств;
- base и overlay данные свойств;
- `ExplicitlyPersistent`;
- ID и записи пользовательских holder;
- производное от прототипа runtime-состояние.

Не добавляйте в `Entity` или `Properties` предположения конкретной базы данных, если их нельзя поддержать всеми backend и тестами.

## Метрики и диагностика

`GetDbRequestsPerMinute()` сообщает недавнее число запросов к базе через посекундные bucket. Ошибки backend и попытки reconnect учитываются в состоянии `DataBaseImpl`.

`DrawGui()` доступен на уровнях фасада и backend для отладки и проверки.

## Тесты для проверки

Подходящие тесты:

- `Source/Tests/Test_DataBase.cpp`;
- тесты управления сущностями, например `Test_LocationAndEntityMgmt.cpp`, когда persistence влияет на сохраняемые сущности;
- зависящие от backend тесты при включённых сборочных параметрах.

## Маршрутизация изменений

- Открытый фасад и общая логика commit/recovery: `Source/Server/DataBase.h` и `Source/Server/DataBase.cpp`.
- JSON backend: `Source/Server/DataBase-Json.cpp`.
- SQLite backend: `Source/Server/DataBase-SQLite.cpp`.
- Общий BSON allocator и преобразование: `Source/Server/DataBase.cpp`, `InitializeBsonMemory()`, `DocumentToBson()`, `BsonToDocument()`; Mongo-операции остаются в `Source/Server/DataBase-Mongo.cpp`.
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
