---
layout: default
title: Persistence
locale: en
document_id: persistence
permalink: /Docs/en/explanation/persistence/
---

# Persistence

This document explains the server-side database abstraction, collection/key model, commit queue, recovery logs, and backend implementations.

Use it when changing `Source/Server/DataBase.*`, database settings, entity save/load code, or persistence tests.

## Ownership model

The engine owns the reusable database abstraction, backend implementations, and source-backed recovery boundaries. [Backup and Recovery](../../how-to/release/backup-and-recovery.md) defines the provider-neutral backup/restore procedure; an embedding project owns deployment choices, connection strings, provider tooling, schedules, retention, RPO/RTO, data migration policy, and production authority.

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
- `CreateSQLiteDataBase()` when `FO_HAVE_SQLITE` is enabled;
- `CreateMongoDataBase()` when `FO_HAVE_MONGO` is enabled;
- `CreateMemoryDataBase()`.

Implementation files:

- `Source/Server/DataBase-Json.cpp`
- `Source/Server/DataBase-SQLite.cpp`
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

When operation logging is enabled, `DataBase.OpLogPath` must be non-empty and end with the exact `.oplog` suffix. `InitializeOpLogs()` rejects any other path before opening files. The configured path owns the pending log; the committed-progress log is derived by replacing that final suffix with `-committed.oplog`. Keep the suffix contract intact rather than relying on a broad substring replacement in an arbitrary filename.

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
- SQLite backend: enabled only when the build has `FO_HAVE_SQLITE`, which is server-only — clients link no embedded database. Every collection is a table inside one `Storage.sqlite` file, journalled in WAL mode, and SQLite allocates through the engine memory system via `SQLITE_CONFIG_MALLOC`.
- Mongo backend: enabled only when the build has `FO_HAVE_MONGO`; it shares the BSON conversion and allocator setup used by the JSON and SQLite backends.
- Memory backend: useful for tests and non-durable runtime paths.

`DocumentToBson()` and `BsonToDocument()` convert between `AnyData::Document` and the BSON payload used by JSON, SQLite, and Mongo storage. `GetDbKeyType()` reports whether a runtime key is integer- or string-backed.

## Relationship to entity state

Persistence stores documents; entity state reaches those documents through property serialization and server entity-management code.

Relevant entity/property concepts from [Entity Model](../entity-and-property-model/):

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
- SQLite backend: `Source/Server/DataBase-SQLite.cpp`.
- Shared BSON allocator/conversion: `Source/Server/DataBase.cpp`, `InitializeBsonMemory()`, `DocumentToBson()`, and `BsonToDocument()`; Mongo-specific operations stay in `Source/Server/DataBase-Mongo.cpp`.
- Memory backend: `Source/Server/DataBase-Memory.cpp`.
- Entity/property serialization: [Entity Model](../entity-and-property-model/) and `PropertiesSerializer.*`.
- Build feature toggles: [Build Workflow](../../how-to/build/) and [BuildTools Pipeline](../../reference/cmake-and-buildtools/pipeline.md).
- Backup sets, oplog limits, isolated restore, and disaster-recovery drills: [Backup and Recovery](../../how-to/release/backup-and-recovery.md). Deployment stop/rollback sequencing: [Release Operations](../../how-to/release/operations.md). Provider choice, schedules, retention, RPO/RTO, concrete migrations, and production authority remain project-owned.

## Validation checklist

1. Run `Source/Tests/Test_DataBase.cpp` or the embedding project's equivalent database test target.
2. Validate insert, update, delete, get, valid, and ID enumeration for every affected backend.
3. Validate integer-key and string-key collections when changing key handling.
4. Validate commit queue drain with `StartCommitChanges()` / `WaitCommitChanges()`.
5. Validate operation-log restore after a simulated failed commit when durability/recovery behavior changes.
6. Validate that an enabled `DataBase.OpLogPath` is non-empty and ends with `.oplog`, and that the derived `-committed.oplog` path cannot collide with the pending log.
7. Validate entity save/load paths when persistent property semantics change.
8. Never put production credentials or live connection strings into repository docs or tests.
