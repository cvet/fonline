---
layout: default
title: Persistence
locale: en
document_id: persistence
permalink: /Docs/en/explanation/persistence/
---

# Persistence

This document explains the server-side database abstraction, collection/key model, commit queue, backend-consistent snapshots, recovery logs, and backend implementations.

Use it when changing `Source/Server/DataBase.*`, database settings, entity save/load code, or persistence tests.

`DataBase-Json.cpp` publishes complete inserted and updated JSON records through `fs::write_file`; unique sibling staging replaces the former fixed `<record>.tmp` sequence. Record keys, JSON content and commit-queue semantics remain unchanged. SQLite snapshot restore uses the same atomic helper for its unpublished scratch database before opening it for restore.

## Ownership model

The engine owns the reusable database abstraction, backend implementations, and source-backed recovery boundaries. [Backup and Recovery](../../how-to/release/backup-and-recovery.md) defines the provider-neutral backup/restore procedure; an embedding project owns deployment choices, connection strings, provider tooling, schedules, retention, RPO/RTO, data migration policy, and production authority.

Do not put live credentials, production connection strings, or host-specific recovery steps in this engine document.

## Graceful shutdown and entity ownership

Persistent entity-holder properties describe durable ownership. `ServerEngine::Shutdown` must leave those IDs and custom-entity documents intact. `EntityManager::DestroyAllEntities` clears runtime inner references while the registry still retains every entity, then drops parent links and registry ownership. This is distinct from ordinary `DestroyCustomEntity`, which updates holder IDs and deletes documents. The distinction applies to Game-owned trees and custom children of standard holders alike. `ServerShutdownPreservesPersistentCustomEntityTreesAcrossRestarts` exercises the boundary through two real file-backed restarts; a live-world read before shutdown is not durability evidence.

## Public database facade

`Source/Server/DataBase.h` defines the public facade `DataBase`. It wraps a `DataBaseImpl` backend and exposes collection/document operations:

- state/metrics: `InValidState()`, `GetDbRequestsPerMinute()`;
- enumeration: `GetAllIds()`, `GetAllIntIds()`, `GetAllStringIds()`;
- reads: `Get()`, `GetMany()`, `Valid()`;
- writes: `Insert()`, `TryInsert()`, `Update()`, `CompareExchange()`, `Delete()`;
- commit control: `StartCommitChanges()`, `WaitCommitChanges()`, `ClearChanges()`;
- backend snapshot: `CreateSnapshot()` and `RestoreSnapshot(bytes)`;
- debug UI: `DrawGui()`.

`ConnectToDataBase()` constructs the facade from settings, connection info, collection schemas, and a panic callback. JSON and `DbSQLite` use the trimmed entire remainder after the provider name as their storage directory, so `JSON C:/Game Data/world` and `DbSQLite C:/Game Data/world` preserve internal spaces. Do not put literal quote characters around that remainder. Empty directories fail; Memory and Mongo retain their token grammar.

`GetMany(collection, ids)` reads several records of one collection through one backend `GetRecords()` request and returns documents aligned with the requested ids: a missing record produces an empty document and a repeated id repeats the same result. Each document follows the `Get()` contract, with the uncommitted journal suffix and then pending commit operations overlaid. A conflicting pending insert is reported rather than replacing an unrelated stored document. Changes to either journal during the read cause a retry; a record committed during the batch read is fetched again. `Get()` itself uses the same path for one id.

Active readers share a record generation, with each backend request capturing its own expected generation. Commit advances it under the same lock that removes the pending operation. Another reader cannot replace an invalidated read's evidence of that commit. The tracking entry is removed only when its last reader leaves, so completed historical records do not accumulate. `DataBaseConditionalReadCannotBorrowAnotherReadersCommitMarker` pins the two-reader/commit ordering as well as CAS refusal of the old value.

Each successfully processed recovery command also advances the affected record generation; insert and delete advance the collection key generation. This includes an idempotent replay, so a successful retry after a partially applied backend operation invalidates an earlier read or listing. It does not make backend mutation and bookkeeping atomic or serialize concurrent recovery and writes.

Replay also reads a per-backend-instance storage revision with the record and conditionally mutates under that backend's storage lock. A mutation advances the revision before it may change storage, so a stale replay retries even if intervening writes restored the original value. An unrelated write can also force a retry. This process-local check does not coordinate independent database owners or external writers, order a later commit behind an older journal, or make journal progress atomic with storage.

`GetAllIds(collection)` and its typed `GetAllIntIds()` / `GetAllStringIds()` variants enumerate backend keys with inserts and deletes from the uncommitted journal suffix and then the pending queue overlaid in order. A pending delete removes a listed key; a pending insert adds it. A journal change or an insert or delete committed during backend listing causes a retry. Updates do not change the key set.

## Conditional mutations

`TryInsert(collection, id, document)` returns `false` when the logical record already exists. `CompareExchange(collection, id, key, expected, desired)` changes an existing field only when its typed value equals `expected`; a missing record, missing field or mismatch returns `false` without queuing a write. Both include pending inserts, updates and deletes in the condition. Invalid collection/key types, empty insert documents and non-finite desired values still throw.

One producer mutex per `DataBaseImpl` holds the condition check and enqueue together and serializes them with ordinary inserts/updates/deletes, `RestoreSnapshot()` and `ClearChanges()`. The commit thread takes only its existing queue/backend locks. A `true` result means an operation entered the logical queue, not that it was durably committed: `StartCommitChanges()` enables processing and `WaitCommitChanges()` retains its existing failure/recovery limits. This is neither a cross-instance/cross-process transaction nor a backend-native Mongo/SQLite compare-and-swap. Consumers must keep a single owning instance for records needing this guarantee.

Server scripts expose `Game.DbTryInsertRecord(collection, id, keyValues)` and `Game.DbCompareExchangeRecordString(collection, id, key, expected, desired)` for both integer and string ids. These string-field methods validate collection/id, nonempty insert values/key and UTF-8 before delegating to the shared database path. They require no live entity cover. Existing `DbInsertRecord` duplicate-error behavior is unchanged.

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

- `GetRecords()` for a batched read. Mongo uses `_id: {$in: [...]}` chunks of at most 1000 ids with matching batch size/limit, SQLite uses one `key IN (...)` statement per 1000 ids, and Memory/JSON read the batch under one storage lock. The base implementation falls back to one `GetRecord()` per id;
- `CreateSnapshotData()` and `RestoreSnapshotData()` when the backend can represent its whole content as bytes;
- `TryReconnect()`;
- `DrawGui()`;
- test hooks such as `OnCommitOperationWrittenToOpLog()` and `OnPendingChangesRestored()`.

Provider factories live in adjacent guarded headers and source pairs:

- `DataBaseMemory::Create()` in `DataBase-Memory.h`, always available;
- `DataBaseJson::Create()` in `DataBase-Json.h`, with `FO_DATABASE_JSON`;
- `DataBaseSQLite::Create()` in `DataBase-SQLite.h`, with `FO_DATABASE_SQLITE`;
- `DataBaseMongo::Create()` in `DataBase-Mongo.h`, with `FO_DATABASE_MONGO`.

`DataBase.cpp` owns dispatch, commit/recovery, keys and metrics. It includes every provider header unconditionally; a disabled provider has no declarations or implementation. `DataBaseSerialization.h/.cpp` owns the neutral JSON/CBOR document conversion. See [component selection](../../how-to/build/project-configuration.md#optional-native-components).

## Commit queue

Writes are represented as commit operations:

- `Insert`
- `Update`
- `Delete`

`DataBaseImpl` queues pending commit operations and processes them through commit-thread machinery:

- `StartCommitChanges()` schedules/starts commit processing;
- `WaitCommitChanges()` waits for the queue to drain, or returns when the backend becomes invalid without guaranteeing pending writes are durable;
- `ClearChanges()` discards operations that have not started; an operation already handed to the backend remains in the queue and read overlay until it finishes, and later writes remain queued;
- `CommitNextChange()` applies one operation;
- `CommitThreadEntry()` runs the background loop.

The public `DataBase` facade forwards write calls into this machinery. Backend write implementations should remain focused on durable record operations, while shared logic handles scheduling, operation logs, panic/retry policy, and metrics.

A backend read or write failure wakes commit waiters. A successful recovery attempt clears its failure state only if no newer backend failure was observed during that attempt.

## Backend-consistent snapshots

`DataBase::CreateSnapshot()` returns the whole database content as bytes and `RestoreSnapshot(bytes)` puts such a content back. The database never names a file, creates a directory, or decides where a snapshot lives: storing the bytes is the caller's business. Shared `DataBaseImpl` logic requires an active commit thread for a capture, drains the pending commit queue, rejects a failed backend, then blocks new `Insert()` / `Update()` / `Delete()` producers until the backend operation returns or throws. A restore requires an already drained queue and blocks producers the same way. Concurrent snapshot operations serialize. Reads may continue, subject to the backend's own storage lock.

A backend without a byte representation throws `DataBaseException` in both directions, so an unsupported backend fails loudly instead of returning an empty snapshot. These operations do not select slot names, publish packages, write manifests, capture Engine/runtime state, or authorize a save; those policies belong to the embedding session controller. The caller must first stop gameplay/runtime producers and materialize any exact state that needs to enter the commit queue.

`DbSQLite::CreateSnapshotData()` serializes the database with `sqlite3_serialize()` while holding the source storage lock, so the bytes are the exact page image the database would have on disk with WAL content folded in. `RestoreSnapshotData()` loads those bytes into a private in-memory database with `sqlite3_deserialize()` and copies it into the live storage with the online-backup API: deserializing straight into the live handle would detach it from its file. The buffer handed to `sqlite3_deserialize()` is allocated with `sqlite3_malloc64()` and freed on close.

`ServerEngine::CreateSnapshot()` composes these bytes with format-4 runtime state under whole-world quiescence. Named time events, their issued-id boundary and restorable movements (including leases) are serialized; delegate callbacks, unfinished script work and unsupported owners block capture. Native projects can add eligibility diagnostics for their own state. See [server pause and snapshots](../runtime/server.md#initialization-and-server-jobs) for ownership, restore ordering and strict format validation. This remains separate from atomic slot publication and does not serialize transport or arbitrary project memory.

`DataBaseSnapshotDrainsAndBlocksNewProducers` pins the shared commit/producer barrier. `SQLiteDataBaseCreatesReopenableSnapshotWithoutOverwriting` pins pending-write inclusion, later-source-mutation exclusion, live-source independent reopen, no-overwrite behavior, and a failed destination that leaves no database file.

## Recovery logs and panic policy

`DataBaseImpl::RecoveryLogHandle` owns an operation-log file:

- `GetPath()`;
- `IsUsable()`;
- `GetLinesCount()`;
- `GetTextSize()`;
- `SnapshotContent()`;
- `Append()`;
- `Truncate()`.

The handle serializes its individual reads, appends, and truncations. `IsUsable()` waits for a concurrent file operation before reporting whether an I/O failure made the handle unusable; an operation still in progress is not itself a failure. `SnapshotContent()` checks usability under the same lock and rejects a handle whose I/O failed, rather than returning a potentially stale copy. A successful snapshot is owned, so a later append or truncate cannot invalidate its lines. These individual operations do not coordinate the two logs, backend writes, or concurrent recovery operations.

`AppendIfPending()` checks for existing pending lines and appends under the same lock, returning whether the journal was empty, the command was appended, or the operation failed. With a non-empty journal it rejects an empty command and an interrupted cleanup marker. A healthy commit that appends to an existing journal replays the journal in order, including its own command, instead of issuing a second backend write. If replay fails, the backend is marked failed and recovery follows the existing retry path; the command is not appended twice. An unusable journal remains an explicit recovery failure. `DataBasePreloggedReplaySurvivesFailureAfterEitherMutation` checks failure after both an older and a newer mutation and recovery after restart.

Calling `RestorePendingChanges()` synchronously from the same database's backend insert, update, or delete raises `DataBaseException` when operation logging is enabled. Callbacks after a completed spill or recovery remain outside this boundary. Rejecting reentry alone does not order an already applied backend write against older outstanding log entries.

`DataBaseImpl` can keep pending and committed change logs:

- `_pendingChangesLog`
- `_committedChangesLog`

After replay, cleanup writes a `@reset-v2` marker to pending and then committed while holding the pending log lock, before truncating pending. On reopening, a pending marker identifies the already applied prefix; a committed marker without a pending marker identifies a completed pending truncation, so any pending lines are new commands. The previous `@reset-v1` marker with a non-empty pending log is ambiguous and recovery rejects it without truncating either log. Older binaries do not understand these markers and may reject an interrupted cleanup until the newer binary completes it. This cleanup protocol does not serialize concurrent recovery or order replay against backend writes.

When pending grows during replay, recovery can continue with the appended suffix after verifying the original prefix and that no truncation occurred. A revision check under the pending log lock prevents cleanup from deleting an append that arrives after the snapshot. If another recovery has truncated pending or started marked cleanup, the stale pass restarts with fresh snapshots; malformed markers and mismatched contents remain errors. This does not resolve concurrent replay or backend writes to the same record.

The normal replay cleanup checks both log revisions before writing its markers. Resuming with a pending marker checks both revisions before completing the markers and truncating pending. In both paths the final committed-log truncation is conditional on the revision of that cleanup's marker, so it does not erase progress written by another recovery after pending was cleared. Other recovery and backend ordering boundaries remain separate.

For the legacy `@reset-v1` marker with an empty pending log, cleanup checks both revisions and truncates both logs while holding their locks. A concurrent append cannot enter between the two truncations and be mistaken for part of the legacy cleanup.

Before interpreting the initial snapshots and cleanup markers, recovery checks that both handles remain usable at the captured revisions. It retries the snapshot if another recovery has changed either log. Clearing a committed-only `@reset-v2` marker also checks its revision and takes fresh snapshots afterwards. This initial retry does not serialize the subsequent replay.

After reading a record for replay, recovery also checks whether either log changed during that read. If so, it starts again from current logs before applying the stale command. The log-revision check and backend mutation are not one atomic operation; the separate backend revision check rejects stale storage snapshots, but simultaneous replay and later commits still need ordering.

When operation logging is enabled, `DataBase.OpLogPath` must be non-empty. The configured path owns the pending log; the committed-progress path is derived by replacing `.oplog` with `-committed.oplog`. `InitializeOpLogs()` does not enforce that suffix, so keep the conventional `.oplog` name and verify that the derived path is distinct rather than relying on an arbitrary filename.

If a log size cannot be represented by `size_t` or exceeds the string implementation limit, opening fails before allocating or reading its contents. If opening or validating either log fails, `InitializeOpLogs()` closes both handles before propagating the failure. The files are not truncated by this cleanup, so a caller can inspect and repair them before retrying initialization.

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

The panic callback runs before the shutdown timer is submitted. If submitting the timer throws, the server reports the failure and terminates immediately rather than continuing with a claimed panic and no timer.

When changing commit durability, validate failed-write recovery and pending-log restoration, not only successful writes.

If an oplog write, seek or truncate fails, that open log handle becomes unusable: its in-memory lines may no longer match the file. Further writes and recovery through that handle fail; reopening reads the file again. This guard does not serialize concurrent recovery with writers or make a failed sync durable.

`BuildTools/tests/test_recovery_log_io_faults.py` compiles the current log mutation methods with controlled I/O failures, including a partial write, and checks that the handle rejects subsequent I/O. This isolated probe does not exercise real filesystem failures, recovery replay or power loss.

## Backend-specific notes

A MongoDB single-record replay read distinguishes an exhausted cursor from a cursor error. A replay update also requires an acknowledged match, even if the value was already unchanged. A read error or an update without a match aborts recovery and leaves the pending journal available for retry. `BuildTools/tests/run_mongo_recovery_faults.py` exercises these outcomes through libmongoc against a loopback wire fixture. The fixture also injects insert and delete errors, verifies the commands reached the server, and checks that the pending journal remains intact. BSON used by those writes is released on error. The test executable must be built with `FO_DATABASE_MONGO` enabled. The fixture does not replace a real MongoDB restart test.

```text
python BuildTools/tests/run_mongo_recovery_faults.py --test-exe <test-executable> --project-dir <test-project-directory> --output-dir <task-workspace>/mongo-recovery
```


- JSON backend: file/directory-oriented storage and string-key escaping suitable for filesystem paths. A missing record is empty only when its collection directory exists; an invalid collection path, dangling record link, or unreadable record raises a database error.
- SQLite backend: enabled only when the build has `FO_DATABASE_SQLITE`, which is server-only — clients link no embedded database. Every collection is a table inside one `Storage.sqlite` file, journalled in WAL mode, SQLite allocates through the engine memory system via `SQLITE_CONFIG_MALLOC`, and `CreateSnapshot()` returns the serialized page image rather than copying the live files.
- Mongo backend: enabled only when the build has `FO_DATABASE_MONGO`; only this provider owns the BSON conversion and allocator setup, in `DataBase-Mongo.cpp`.
- Memory backend: useful for tests and non-durable runtime paths.

`DocumentToBson()` and `BsonToDocument()` are private Mongo helpers. JSON uses ordinary JSON text and SQLite uses CBOR BLOBs through `DataBaseSerialization`. `GetDbKeyType()` reports whether a runtime key is integer- or string-backed.

## Storage formats and upgrade boundary

JSON files contain ordinary UTF-8 objects; SQLite record BLOBs contain CBOR objects. Both preserve signed Int64 (including limits and values above 2^53), finite Float64, Bool, String, Array and Dict, including embedded NUL in strings/keys. Null/binary, unsigned overflow, non-object roots, malformed/truncated input and trailing CBOR bytes fail. Limits: 64 MiB per record and 64 nesting levels. CBOR SAX validation checks types/depth/declared lengths before DOM allocation; validation precedes writes.

Use new empty JSON/SQLite storage. Old Extended JSON/BSON BLOBs have no reader, conversion or fallback; keep old binaries/storage together for rollback. Mongo BSON and Memory semantics remain unchanged. SQLite retains tables/keys, WAL and snapshot/restore; Engine snapshots require current metadata and binaries matching the compatibility marker in `Source/Common/Common.h`. [Component selection](../../how-to/build/project-configuration.md#optional-native-components) owns defaults, roles/dependencies and disabled-provider errors; [backup/recovery](../../how-to/release/backup-and-recovery.md) owns consistent sets and isolated restores. Guarded `Test_DataBase.cpp` fixtures cover types, integer/NUL/depth/corruption, provider reopen/update/delete, snapshots and recovery/oplog paths.

## Relationship to entity state

Persistence stores documents; entity state reaches those documents through property serialization and server entity-management code.

A failed critter restoration reports the error but does not save persistent server-entity holder ID lists pruned to its partial runtime graph. Missing child documents therefore remain retryable without losing durable references. Successful pre-load handlers and explicit migrations still persist their changes normally.

Relevant entity/property concepts from [Entity Model](../entity-and-property-model/):

- persistent property flags;
- temporary property exclusion;
- base/overlay property data;
- `ExplicitlyPersistent`;
- custom holder IDs/entries;
- prototype-derived runtime state.

Do not add database-specific assumptions to `Entity` or `Properties` unless all backends and tests can support the behavior.

## Metrics and diagnostics

`GetDbRequestsPerMinute()` reports recent database request volume using per-second buckets; a batch counts once per backend `GetRecords()` call, regardless of its record count. Backend failures and reconnect attempts are tracked in `DataBaseImpl` state.

`DrawGui()` is available at both facade and backend levels for debug/inspection UI.

## Tests to inspect

Relevant tests include:

- `Source/Tests/Test_DataBase.cpp`
- entity-management tests such as `Test_LocationAndEntityMgmt.cpp` when persistence affects saved entities;
- backend-specific tests when enabled by build options.

The hidden `MongoDataBaseRecoversAfterIsolatedServerRestart` test requires `FO_TEST_MONGO_URI` (loopback) and `FO_TEST_MONGO_CONTROL` (an isolated writable directory). A coordinator waits for `ready`, stops its owned mongod, writes `stopped`, waits for `spilled`, restarts that instance and writes `restarted`; `complete` confirms replay and reopen.

## Change routing

- Public facade and shared commit/recovery logic: `Source/Server/DataBase.h` and `Source/Server/DataBase.cpp`.
- JSON backend: `Source/Server/DataBase-Json.cpp`.
- SQLite backend: `Source/Server/DataBase-SQLite.cpp`.
- Mongo-only BSON allocator/conversion: `Source/Server/DataBase-Mongo.cpp`, `InitializeBsonMemory()`, `DocumentToBson()`, and `BsonToDocument()`; Mongo-specific operations stay in `Source/Server/DataBase-Mongo.cpp`.
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
6. Validate that an enabled `DataBase.OpLogPath` is non-empty and that replacing `.oplog` with `-committed.oplog` produces a distinct committed-log path.
7. For snapshot changes, prove pending changes are included, later source mutations are excluded, the destination reopens while the source remains live, completed destinations are not overwritten, and failed destinations leave no database artifact.
8. Validate entity save/load paths when persistent property semantics change.
9. Never put production credentials or live connection strings into repository docs or tests.
