---
layout: default
title: Release Operations
locale: en
document_id: release-operations
permalink: /Docs/en/how-to/release/operations.html
---

# Release Operations

This guide covers reusable FOnline server lifecycle, readiness, rollout, shutdown, and rollback. [Packaging and Release](packaging.md) owns artifacts; [Persistence](../../explanation/persistence/) owns database mechanics; [Backup and Recovery](backup-and-recovery.md) owns the provider-neutral recovery procedure; the game owns infrastructure, data policy, objectives, and incidents.

## Rollout decision

Run the foreground headless binary under a real supervisor. A detached daemon's
parent exits before the child completes startup, so its successful parent exit
is not readiness evidence. Readiness requires `Start server complete!` and a
project-owned functional probe; a merely live process is explicitly not ready.

Stop gracefully with `SIGTERM` on POSIX or `SERVICE_CONTROL_STOP` for the
Windows service and wait for `Server stopped!`. The worker drain setting bounds
only the first worker-pool drain, not the total stop timeout. Rollback is a new
controlled deployment of one data-compatible immutable artifact/config/data
unit through the same readiness gate. Never mix binaries, resources, config,
or data from incompatible release units.

## Establish the operating boundary

The Engine emits binaries, not service accounts, supervisors, containers, traffic/TLS/firewall policy, database clusters, backups, alerts, or on-call policy. Version those in the project.

An immutable release unit contains the accepted server binary, baked resources/config, client packs, native updater payloads, and manifest. Keep writable state outside it when atomic replacement or rollback retention requires that split.

The optional health file uses an executable-derived name and remains in the
working directory. The log initially opens there too, but only the log moves
below the resolved `Client.UserWritablePath` after settings load when that value
is non-empty. Set the working directory and any writable-path override
explicitly; launch environments need not choose the same locations.

## Choose the server process

| Binary | Runtime behavior | Operational use |
|---|---|---|
| `<Game>_Server` | Windowed server with the application frontend | Local development and attended diagnosis |
| `<Game>_ServerHeadless` | Foreground, no rendering; waits until quit or startup failure | Preferred process under an external supervisor or container |
| `<Game>_ServerService` | Windows-only SCM integration; reports `RUNNING` after `ServerEngine::IsStarted()`, stops cleanly on startup failure, and handles `SERVICE_CONTROL_STOP` | A project-qualified native Windows service route |
| `<Game>_ServerDaemon` | Non-Windows process calls `fork()`, closes standard streams and calls `setsid()` in the child; the current app ignores the returned failure flag and can continue in the original process when `fork()` fails | Legacy detached launch requiring project qualification, PID ownership, and child monitoring |

Prefer the foreground headless binary under a supervisor. The daemon's parent exits before the child completes startup, so successful launch exit is not readiness evidence; it writes no PID file and implements no manager protocol.

The Windows helper registers fixed SCM name `FOnlineServer` as demand-start. Running the service executable without a recognized control flag registers or updates a command composed from the quoted executable path, the current command line, and `--server-service`; `--server-service-delete` removes the registration, while `--server-service-start` enters the SCM dispatcher. The registered command does not itself use that start flag, so treat the current helper as requiring Windows qualification rather than assuming registration proves service startup. It configures no working directory, account, dependencies, recovery actions, or product-specific name.

## Define readiness and health

Process existence is liveness, not readiness. A reusable readiness gate requires all of these observations:

1. The process remains alive and has not reported startup failure.
2. The log reaches `Start server complete!` after runtime, world, scripts, and initial commit succeed.
3. A project-owned functional probe, such as a compatible client handshake/login, succeeds through the real network route.

With `Server.WriteHealthFile = True`, startup writes `Starting...` to `<executable>_Health.txt`; after `_started`, the periodic writer replaces it with version, time, load, connection, entity, job, rejection, and database metrics. Therefore:

- `Starting...` is explicitly not ready;
- require the expected version and compatibility version, a parseable body, and a recent modification time;
- treat a stale file as unknown or unhealthy, not as proof that the process is dead;
- set `Server.HealthFilePeriodMs` to a positive project-qualified interval;
- expose the local file through a project-owned probe or sidecar when an orchestrator needs a network endpoint.

The Engine provides no HTTP health/drain endpoint or alert. Do not publish the file to an untrusted network.

## Prepare a deployment

Before changing a running environment:

1. Identify the exact game and Engine SHAs, package/config ID, artifact hashes, signing result, updater generation/runtime ABI, and database/schema compatibility.
2. Verify the artifact from its immutable store, scan its inventory for secrets, and stage it without modifying the active release.
3. Resolve target-time configuration and credentials on the target host according to [Security and Secrets](security-and-secrets.md).
4. Validate executable, resource, writable-directory, port, firewall, database, and service-account permissions without printing secrets.
5. For durable-state changes, follow [Backup and Recovery](backup-and-recovery.md): capture a backend-consistent backup/snapshot, preserve both recovery oplogs, and prove an isolated semantic restore. An Engine operation log is not a substitute for a backup.
6. Confirm the previous compatible artifact/config/data unit is still available and name the operator authorized to roll back.
7. Run the same start/readiness/smoke/stop sequence in a representative non-production environment.

Never point old and new servers at the same writable database concurrently unless the game's persistence design and migration tests explicitly prove that topology.

## Roll out and verify

Use a staged rollout even when the final topology has one server:

1. Remove or isolate the instance from new traffic through project infrastructure. FOnline has no built-in drain protocol.
2. Request a graceful stop and wait for the process to exit; do not overwrite files used by a live process.
3. Activate the immutable release directory or versioned image and its matching configuration.
4. Start under a bounded restart policy; repeated startup failure is an incident.
5. Require the complete readiness gate. A Windows `SERVICE_RUNNING` state is useful first-party evidence, but still pair it with a functional probe.
6. Restore traffic gradually; watch restarts, health freshness, connections/rejections, jobs, database/updater errors, and game indicators.
7. Record manifest, environment/config, run identity, timestamps, evidence, and disposition.

Before traffic returns, check [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md). Unsupported updater generations require a new full client package.

## Stop safely

On Linux/macOS, send `SIGTERM` or `SIGINT` to headless or the daemon child; the loop converts the latched signal to quit. On Windows service use `SERVICE_CONTROL_STOP`. Forced termination bypasses Engine shutdown.

`ServerEngine::Shutdown()` stops networking/jobs, fires `OnFinish`, destroys entities/backends, flushes identity/time, waits for database commits, disconnects players, and logs `Server stopped!`.

`Server.ShutdownGraceMs` bounds only the first worker-pool drain. Then parked lock waiters are aborted and an unbounded wait resumes; scripts, `OnFinish`, backends, and commits can extend stop. Size manager timeout from measured worst case; forced kill may lose pending state.

Accept a graceful stop only when `Server stopped!` is present and the process exits successfully. Preserve logs and investigate a timeout or crash before replacing its evidence.

## Roll back

Rollback is a new controlled deployment, not a file copy over a running process:

1. Freeze rollout and remove the affected instance from traffic.
2. Preserve logs, health/crash evidence, release/config identity, and needed database state.
3. Stop the process gracefully and verify exit.
4. Decide whether binary/config rollback is data-compatible. If not, execute the tested [backup/restore](backup-and-recovery.md) or game-owned forward-fix procedure before starting the old server.
5. Activate the previous artifact with matching config/updater payloads and repeat readiness probes.
6. Restore traffic gradually, monitor, and record the incident and final release state.

Restore data only under project consistency/data-loss policy. Never mix binaries, baked resources, config, client packs, native updater payloads, or data from incompatible release units.

## Failure routing

| Observation | Route |
|---|---|
| Process exits before readiness or logs `Server startup failed, shutting down` | Keep traffic closed; inspect the first startup exception, config, resources, database, and permissions |
| Health file remains `Starting...` | Startup is incomplete; use logs and process state, not the file as a ready signal |
| Health file is stale after readiness | Check permissions/scheduling/writer diagnostics; keep state unknown until a functional probe passes |
| Daemon launch command exits zero but no ready child appears | Inspect the child log/process; parent exit is expected and is not startup evidence |
| Graceful stop exceeds the supervisor timeout | Keep evidence, inspect the last `Shutdown stage:` marker, database availability, and game `OnFinish`; do not assume `ShutdownGraceMs` is a total bound |
| New server is ready but clients cannot update or connect | Hold rollout and reconcile network, compatibility, updater generation/runtime ABI, and packaged client payloads |
| Rollback binary cannot read current durable state | Do not start it against production data; follow the game-owned migration/restore decision |

## Validate the runbook

Automate exact-package install, premature-readiness rejection, ready log/health transition, real handshake/login, graceful stop, `Server stopped!`, and successful exit. Inject startup and unavailable-database failures.

Run [Examples/PackagingMatrix](../../../../Examples/PackagingMatrix/) plus project infrastructure/persistence lanes. Rehearse rollback with immutable artifacts and synthetic data.

## Source paths inspected

`Source/Applications/Server{App,HeadlessApp,ServiceApp,DaemonApp}.cpp`, `Source/Frontend/Application{Init,Headless}.cpp`, `Source/Essentials/Platform.cpp`, `Source/Server/Server.cpp`, `Source/Common/Settings.inc`, `BuildTools/cmake/stages/Applications.cmake`, `BuildTools/package.py`, `BuildTools/tests/test_docs_release_operations.py`, and `Examples/PackagingMatrix`.
