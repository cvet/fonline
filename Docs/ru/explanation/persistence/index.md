---
layout: default
title: Сохранение данных
locale: ru
document_id: persistence
permalink: /Docs/ru/explanation/persistence/
---

# Сохранение данных

<!-- docs-translation: {"document_id":"persistence","locale":"ru","source_path":"Docs/en/explanation/persistence/index.md","source_sha256":"0cc2d410296794ff1b79efe7db64f834c30841f95e0d6c22f01ca02d5076308c"} -->

Этот документ описывает серверную абстракцию базы данных, модель коллекций и ключей, очередь commit, журналы восстановления и реализации backend.

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
- SQLite backend: доступен только при `FO_HAVE_SQLITE` и только серверу — клиент не связывает встроенную базу. Каждая коллекция является таблицей в едином `Storage.sqlite`, журналируемом в WAL; SQLite выделяет память через систему памяти движка посредством `SQLITE_CONFIG_MALLOC`.
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
7. При изменении семантики постоянного свойства проверьте загрузку и сохранение сущности.
8. Никогда не помещайте производственные учётные данные или действующие строки подключения в документацию или тесты репозитория.
