---
layout: default
title: Backup and Recovery
locale: en
document_id: backup-and-recovery
permalink: /Docs/en/how-to/release/backup-and-recovery.html
---

# Backup and Recovery

This runbook defines the reusable backup, restore, and disaster-recovery boundary for an FOnline server. Use it with [Persistence](../../explanation/persistence/) for storage mechanics, [Release Operations](operations.md) for process control, and the [Engine Upgrade Guide](../migration/engine-upgrade.md) when durable data crosses an Engine or game revision.

## Recovery decision

Identify the complete durable set first: `Memory` has no persistent set; SQLite
needs `Storage.sqlite` and active WAL sidecars; JSON needs the complete storage
tree; Mongo needs a provider-native consistent backup. The recovery oplog is
not a backup: only a command whose backend write reports failure is appended to
the pending log, and the committed file records only the committed prefix of
that pending log. The two files therefore cannot reconstruct successful writes
that were never appended or changes lost through unreported power failure.

For a portable baseline, quiesce traffic, stop gracefully through `Server
stopped!`, capture the backend plus both oplog files, and call the result only a
backup candidate until it restores in an isolated environment. Never mix
binaries, resources, and data across incompatible release units. Require `Start
server complete!`, a project-owned semantic probe, and a new write/read cycle
that proves the write survived. Rehearse from the off-site copy, measure RPO/RTO,
record semantic checks, and assign corrective action for every missed objective.
The Engine exposes no online backup or checkpoint command; provider-native
SQLite or Mongo procedures must come from the reviewed project runbook.

## Establish the recovery boundary

The Engine owns the database facade, JSON/SQLite/Mongo/Memory backends, asynchronous commit queue, recovery oplogs, startup replay, panic callback, and graceful commit drain. An embedding game or its operator owns:

- the selected backend, storage location, Mongo topology, and connection options;
- collection schemas, data migrations, and compatibility between data and code;
- backup provider, schedule, retention, encryption, replication, and off-site policy;
- recovery point objective (RPO), recovery time objective (RTO), service dependencies, traffic drain, and restore authority;
- production credentials, personal-data handling, incident decisions, and destructive-action approval.

The Engine has no database snapshot command, online-backup API, point-in-time recovery controller, migration transaction, traffic-drain endpoint, or restore orchestrator. A project must supply and test those operations without presenting them as built-in Engine behavior.

Treat one recoverable release as a compatible unit: immutable server package, effective configuration and sub-config identity, Engine and game revisions, baked resources, persistent data, recovery oplogs, migration state, and the secrets or key identifiers needed to read them. Never restore data into an arbitrary binary merely because both start.

## Identify the durable set

`Server.DbStorage` selects one of these connection forms:

| Backend | Durable data | Consistency and recovery boundary |
|---|---|---|
| `Memory` | None | Process-local test state. It cannot satisfy a persistent backup or recovery objective. |
| `JSON <directory>` | One JSON file per record under collection directories | Each insert/update writes `<record>.json.tmp` and renames it, but operations spanning records are not one transaction. Take a stopped copy or a filesystem snapshot whose consistency has been proven for the complete directory. |
| `DbSQLite <directory>` | `<directory>/Storage.sqlite` plus active SQLite WAL sidecars | The Engine opens WAL mode with `synchronous=NORMAL`; individual writes autocommit. Do not copy only `Storage.sqlite` while the server is running. Use a provider/SQLite-consistent online backup or stop the server and preserve the complete storage directory. The Engine exposes no online backup or checkpoint command. |
| `Mongo <URI> <database>` | The named Mongo database in the configured deployment | The URI/provider defines write concern, replication, snapshot, dump, and point-in-time capabilities; the Engine does not override them. Use a provider-native database-consistent method and record its guarantees. |

The Engine also opens `DataBase.OpLogPath` and a committed-progress file derived by replacing its required final `.oplog` suffix with `-committed.oplog`. Their default names are `DbPendingChanges.oplog` and `DbPendingChanges-committed.oplog`. Paths are relative to the server working directory unless the project makes them absolute. Startup rejects a configured path without the suffix before either file is opened.

Both oplog files belong to capture and incident evidence. Preserve them with the backend snapshot, even when empty. Never edit, merge, reorder, partially copy, or manually truncate them.

## Understand the recovery oplog

The recovery oplog is not a backup, replication stream, audit history, or point-in-time log.

Normal writes go directly from the in-memory commit queue to the backend. A command is appended and flushed to the pending oplog only after a backend write reports failure while `DataBase.OpLogEnabled` is true. The commit queue then removes that command. On a successful reconnect or a later process start, the Engine:

1. validates both files and requires the committed prefix to match the pending prefix;
2. replays only pending commands beyond the committed prefix;
3. appends and flushes each replayed command to the committed file;
4. verifies exact line equality, truncates the committed file, then truncates the pending file.

Replay is deliberately idempotent within narrow rules: an absent delete is accepted, an identical existing insert is accepted, and an update already contained by the stored document is accepted. A conflicting insert, malformed file, mismatched prefix, replay failure, append failure, or failed truncation stops recovery.

With oplog disabled, the first backend write failure starts database panic. With it enabled, the Engine retries according to `DataBase.ReconnectRetryPeriod`; reaching `DataBase.PanicOpLogSizeThreshold` or failing recovery starts panic. Panic requests application shutdown and, after `DataBase.PanicShutdownTimeout`, forces termination.

An oplog cannot reconstruct successful changes made after an older backup, and it cannot cover a power loss that the backend did not report. Never combine a stale snapshot and a later oplog and call the result point-in-time recovery.

## Define the backup contract

Before operating production, record one reviewed policy per environment:

| Required field | What to record |
|---|---|
| Scope | Backend identity, complete storage set, both oplog paths, external files needed by game-owned persistence, and explicit exclusions |
| Consistency method | Graceful-stop copy, filesystem snapshot, SQLite-consistent backup, or Mongo/provider-native snapshot/dump; include the proven atomicity boundary |
| Recovery objectives | RPO, RTO, backup frequency, replication lag allowance, and maximum acceptable restore age |
| Retention | Rotation classes, off-site copies, legal/privacy expiry, deletion authority, and immutable-copy policy |
| Security | Encryption in transit/at rest, restore-role access, key identifier, audit trail, and redaction rules |
| Compatibility | Engine/game revision, `CompatibilityVersion`, configuration identity, migration/schema version, and supported rollback versions |
| Verification | Hash/inventory checks, plausible size/count comparison with prior recovery points, backend integrity checks, semantic game checks, last isolated restore date, and evidence owner |

Each backup needs a sidecar manifest outside the mutable data set. Record a unique backup ID, UTC start/end, environment, source host/cluster, backend and provider snapshot ID, exact paths/namespaces, file sizes and hashes where applicable, Engine and game commit IDs, package/provenance manifest digest, effective non-secret configuration digest, oplog file sizes/hashes, encryption key ID, consistency method, operator/automation identity, and verification status. Do not put credentials or recovered personal data in this manifest.

## Take a quiesced backup

A stopped backup is the reusable baseline when no reviewed online method exists:

1. Confirm the target environment, backup ID, restore destination, free capacity, retention class, and operator authority. Reject an ambiguous storage path or database name.
2. Stop new sessions and game-owned mutating jobs through project infrastructure. The Engine has no traffic-drain protocol.
3. Request graceful stop through the process route in [Release Operations](operations.md). Do not copy data merely because the process disappeared.
4. Require `Server stopped!`, successful process exit, no critical database failure, and no warning that pending commits could not be guaranteed. If any is absent, switch to the incident capture path below.
5. Capture the backend using the backend-appropriate complete set. Capture both oplog files without modifying them.
6. Create the sidecar manifest, hashes/inventory, and provider completion evidence. Make the backup immutable according to project policy before reopening traffic.
7. Restore the new backup into an isolated environment and run the acceptance checks. A completed copy without a tested restore is only a backup candidate.
8. Start the production release through the normal readiness gate and retain the previous known-good recovery point until the new one passes policy.

An online backup is acceptable only when the backend/provider method gives a documented consistency boundary and the project has restored and validated that exact method under concurrent writes. Process liveness, a filesystem copy tool returning success, or a cloud snapshot being marked complete is not sufficient semantic evidence.

## Capture a database incident

After `Critical database failure`, a forced exit, corrupted storage, or a failed oplog replay:

1. Remove traffic and prevent automated restart loops from mutating evidence.
2. Preserve server logs, crash reports, health evidence, effective non-secret configuration identity, backend state, and both oplog files as one timestamped incident set.
3. Do not start a second server against the same files or database namespace. Oplog handles use exclusive file locking, but that does not protect the backend from every external writer.
4. Do not truncate or repair production data in place. Clone the evidence and investigate the clone.
5. Select a known-good backup whose code/data compatibility and integrity are proven. Treat the incident oplog as replay evidence only; do not assume it fills the interval since that backup.
6. Escalate malformed/mismatched oplogs, insert conflicts, backend integrity failures, or unknown migration state to the project data owner before any production write.

## Restore safely

Restore first to an isolated namespace or host with outbound player traffic and external side effects disabled:

1. Verify backup identity, retention status, signature/hash/inventory, encryption key access, backend version support, and operator approval.
2. Select the exact compatible server package, Engine/game revisions, baked resources, non-secret configuration, and migration level recorded by the backup. Never mix binaries, resources, and data from different release units.
3. Provision an empty, explicitly allowlisted destination. Refuse a restore over the source or only known-good copy. Where the provider supports namespace/path remapping or a dry run, prove the exact destination before the first write; never infer safety from a similar database or directory name.
4. Restore the complete backend set with its native tool. Restore both oplog files to their recorded `DataBase.OpLogPath` locations, preserving names, bytes, ordering, and permissions. Preserve a failed destination for investigation and delete a successful disposable destination only through an exact-name cleanup guard.
5. Run backend-native integrity/consistency checks before starting FOnline. For JSON, also reject leftover `.tmp` files until their origin is understood; for SQLite, validate the complete WAL-aware database; for Mongo, validate the provider restore result and expected database/collections.
6. Start one isolated server. Startup connects the backend, validates oplogs, restores pending commands, loads persistent entities, and only then reaches `Start server complete!`. Treat any startup/replay exception as restore failure.
7. Run a project-owned semantic probe: authenticate a synthetic account, load representative entities and locations, verify critical balances/progress/references, perform a reversible write, stop gracefully, restart, and prove the write survived.
8. Compare expected collection counts/invariants and migration records with the backup manifest. Backend integrity alone cannot prove game semantics.
9. Record actual restore duration, resulting recovery point, data loss against RPO, all commands/tools used, check results, and approver. Promote the restored environment only through the staged rollout/readiness procedure.

Do not let an automatic startup migration modify the first restored copy before an unmodified baseline has been retained. Test forward migration, restart, and any promised rollback against clones.

## Rehearse disaster recovery

At a project-defined cadence, execute a full drill from an off-site or otherwise failure-independent copy. The drill must assume the primary storage is unavailable, obtain required keys through the real emergency path, restore infrastructure and data, start the exact compatible package, run semantic checks, and measure RPO/RTO.

Include at least these failure cases over time:

- missing, expired, corrupt, or partially uploaded backup;
- unavailable encryption key or restore credentials;
- stale backup plus non-empty oplog;
- malformed or prefix-mismatched oplog;
- SQLite WAL sidecar omitted from an unsafe live copy;
- Mongo snapshot with weaker-than-required consistency;
- migration succeeds but rollback cannot read the new data;
- restore is technically healthy but a game-level invariant fails.

A drill passes only when evidence identifies the backup, release unit, restore owner, measured recovery point/time, backend checks, semantic checks, discrepancies, and corrective action. Update the runbook and automation in the same change as a discovered gap.

## Failure routing

| Symptom | Action |
|---|---|
| No `Server stopped!` or pending commits are not guaranteed | Preserve an incident set; do not label the copy quiesced |
| Pending/committed oplogs differ or fail parsing | Stop; preserve both exact files and escalate to Engine/runtime plus project data owner |
| Replay reports a conflicting insert or cannot truncate | Stop restart automation; investigate a cloned backend and oplogs |
| JSON backup contains unexplained `.tmp` files | Treat it as interrupted/inconsistent until source state is proven |
| SQLite live copy omitted WAL state | Reject it; use a complete stopped copy or a proven SQLite-consistent method |
| Mongo restore guarantee is unknown | Reject production promotion until provider consistency and write concern are documented |
| Restored binary cannot read data or migration is one-way | Keep traffic closed and choose a compatible release/backup or approved forward recovery |
| Backend checks pass but semantic probe fails | Keep the restore isolated; route to the owning game schema/system maintainer |
| A backup, log, or manifest exposes a secret | Restrict access, revoke/rotate the credential, preserve sanitized incident evidence, and follow [Security and Secrets](security-and-secrets.md) |

## Validate the runbook

For every supported persistent backend, automate backup creation, isolated restore, integrity checks, server startup, semantic read/write/restart checks, and measured recovery evidence using synthetic data. Test the exact production topology and provider method in the project lane; Engine unit tests prove backend and oplog mechanics, not an operator's backup system.

When persistence or recovery source changes, run `Source/Tests/Test_DataBase.cpp` through the Engine unit-test target. Keep failure-injection coverage for spill-to-oplog, reconnect/replay, invalid records, conflicts, thresholds, and backend round trips. Run the package and release-operations lanes when a recovery unit or process procedure changes.

## Source paths inspected

- `Source/Common/Settings.inc`
- `Source/Server/DataBase.h`
- `Source/Server/DataBase.cpp`
- `Source/Server/DataBase-Json.cpp`
- `Source/Server/DataBase-SQLite.cpp`
- `Source/Server/DataBase-Mongo.cpp`
- `Source/Server/DataBase-Memory.cpp`
- `Source/Server/Server.cpp`
- `Source/Tests/Test_DataBase.cpp`
