# Persistence

This document explains the server-side database abstraction, collection/key model, commit queue, recovery logs, and backend implementations.

Use it when changing `Source/Server/DataBase.*`, database settings, entity save/load code, or persistence tests.

## Ownership model

The engine owns the reusable database abstraction and backend implementations. An embedding project owns deployment choices, connection strings, backup policy, data migration policy, and operational runbooks.

Do not put live credentials, production connection strings, or host-specific recovery steps in this engine document.

## Public database facade

`Source/Server/DataBase.h` defines the public facade `DataBase`. It wraps a `DataBaseImpl` backend and exposes collection/document operations:

- state/metrics: `InValidState()`, `GetDbRequestsPerMinute()`;
- enumeration: `GetAllIds()`, `GetAllIntIds()`, `GetAllStringIds()`;
- reads: `Get()`, `Valid()`;
- writes: `Insert()`, `Update()`, `Delete()`;
- commit control: `StartCommitChanges()`, `WaitCommitChanges()`, `ClearChanges()`;
- debug UI: `DrawGui()`.

`ConnectToDataBase()` constructs the facade from settings, connection info, collection schemas, and a panic callback.

## Collections and keys

The database layer stores `AnyData::Document` values in named collections.

`DataBaseImpl` validates documents recursively before queuing inserts or updates and rejects non-finite `Float64` values in nested documents and arrays. JSON and BSON conversion applies the same rule in both directions, so invalid floating-point data fails at the persistence boundary instead of entering storage or runtime state.

Core types:

- `DataBaseKeyType` — `IntId` or `String`.
- `DataBaseStringKeyEscaping` — `Raw`, `File`, or `Hex`.
- `DataBaseKey` — `variant<ident_t, string>`.
- `DataBaseCollection` — map from `DataBaseKey` to `AnyData::Document`.
- `DataBaseCollectionSchema` — pair of collection name and key type.
- `DataBaseCollectionSchemas` — list of collection schemas used at initialization.

`DataBaseImpl::ValidateCollectionKey()` enforces that collection schemas and record IDs agree. When adding a new persistent collection, add the schema at the server/entity-manager layer and validate all backend implementations.

## Backend interface

`DataBaseImpl` is the backend base class. Backends must implement:

- `GetStringKeyEscaping()`;
- `GetAllRecordIds()`;
- `EnsureCollection()`;
- `GetRecord()`;
- `InsertRecord()`;
- `UpdateRecord()`;
- `DeleteRecord()`.

Backends can override:

- `TryReconnect()`;
- `DrawGui()`;
- test hooks such as `OnCommitOperationWrittenToOpLog()` and `OnPendingChangesRestored()`.

Factory functions declared in `DataBase.h`:

- `CreateJsonDataBase()`;
- `CreateUnQLiteDataBase()` when `FO_HAVE_UNQLITE` is enabled;
- `CreateMongoDataBase()` when `FO_HAVE_MONGO` is enabled;
- `CreateMemoryDataBase()`.

Implementation files:

- `Source/Server/DataBase-Json.cpp`
- `Source/Server/DataBase-UnQLite.cpp`
- `Source/Server/DataBase-Mongo.cpp`
- `Source/Server/DataBase-Memory.cpp`
- shared logic in `Source/Server/DataBase.cpp`

## Commit queue

Writes are represented as commit operations:

- `Insert`
- `Update`
- `Delete`

`DataBaseImpl` queues pending commit operations and processes them through commit-thread machinery:

- `StartCommitChanges()` schedules/starts commit processing;
- `WaitCommitChanges()` waits for the commit thread to drain;
- `ClearChanges()` clears pending state;
- `CommitNextChange()` applies one operation;
- `CommitThreadEntry()` runs the background loop.

The public `DataBase` facade forwards write calls into this machinery. Backend write implementations should remain focused on durable record operations, while shared logic handles scheduling, operation logs, panic/retry policy, and metrics.

## Recovery logs and panic policy

`DataBaseImpl::RecoveryLogHandle` owns an operation-log file:

- `GetPath()`;
- `GetLinesCount()`;
- `GetTextSize()`;
- `GetContent()`;
- `Append()`;
- `Truncate()`.

`DataBaseImpl` can keep pending and committed change logs:

- `_pendingChangesLog`
- `_committedChangesLog`

Recovery/panic settings include:

- `_pendingChangesPanicThreshold`
- `_panicShutdownTimeout`
- `_reconnectRetryPeriod`
- `_panicCallback`

Relevant methods:

- `InitializeOpLogs()`;
- `RestorePendingChanges()`;
- `StartPanic()`;
- `TryReconnect()`.

When changing commit durability, validate failed-write recovery and pending-log restoration, not only successful writes.

## Backend-specific notes

- JSON backend: file/directory-oriented storage and string-key escaping suitable for filesystem paths.
- UnQLite backend: enabled only when the build has `FO_HAVE_UNQLITE`.
- Mongo backend: enabled only when the build has `FO_HAVE_MONGO`; BSON conversion helpers live in the shared header/source.
- Memory backend: useful for tests and non-durable runtime paths.

`DocumentToBson()` and `BsonToDocument()` convert between `AnyData::Document` and BSON for Mongo-backed storage. `GetDbKeyType()` reports whether a runtime key is integer- or string-backed.

## Relationship to entity state

Persistence stores documents; entity state reaches those documents through property serialization and server entity-management code.

Relevant entity/property concepts from [EntityModel.md](EntityModel.md):

- persistent property flags;
- temporary property exclusion;
- base/overlay property data;
- `ExplicitlyPersistent`;
- custom holder IDs/entries;
- prototype-derived runtime state.

Do not add database-specific assumptions to `Entity` or `Properties` unless all backends and tests can support the behavior.

## Metrics and diagnostics

`GetDbRequestsPerMinute()` reports recent database request volume using per-second buckets. Backend failures and reconnect attempts are tracked in `DataBaseImpl` state.

`DrawGui()` is available at both facade and backend levels for debug/inspection UI.

## Tests to inspect

Relevant tests include:

- `Source/Tests/Test_DataBase.cpp`
- entity-management tests such as `Test_LocationAndEntityMgmt.cpp` when persistence affects saved entities;
- backend-specific tests when enabled by build options.

## Change routing

- Public facade and shared commit/recovery logic: `Source/Server/DataBase.h` and `Source/Server/DataBase.cpp`.
- JSON backend: `Source/Server/DataBase-Json.cpp`.
- UnQLite backend: `Source/Server/DataBase-UnQLite.cpp`.
- Mongo backend and BSON conversion: `Source/Server/DataBase-Mongo.cpp`, `DocumentToBson()`, `BsonToDocument()`.
- Memory backend: `Source/Server/DataBase-Memory.cpp`.
- Entity/property serialization: [EntityModel.md](EntityModel.md) and `PropertiesSerializator.*`.
- Build feature toggles: [BuildWorkflow.md](BuildWorkflow.md) and [BuildToolsPipeline.md](BuildToolsPipeline.md).

## Validation checklist

1. Run `Source/Tests/Test_DataBase.cpp` or the embedding project's equivalent database test target.
2. Validate insert, update, delete, get, valid, and ID enumeration for every affected backend.
3. Validate integer-key and string-key collections when changing key handling.
4. Validate commit queue drain with `StartCommitChanges()` / `WaitCommitChanges()`.
5. Validate operation-log restore after a simulated failed commit when durability/recovery behavior changes.
6. Validate entity save/load paths when persistent property semantics change.
7. Never put production credentials or live connection strings into repository docs or tests.
