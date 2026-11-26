---
layout: default
title: Runtime and Tool Log Files
locale: en
document_id: runtime-log-files
permalink: /Docs/en/reference/runtime/log-files.html
---

# Runtime and Tool Log Files

This reference lists the log files that Engine applications and tools write: their names, their location, how they are opened, which of them are rotated, and which settings influence them. It also states plainly what the Engine does not do (level filtering, size limits, age-based deletion) so that a project or operator can add the missing retention policy outside the process.

Operator procedures that consume these files stay in [Release Operations](../../how-to/release/operations.md) and [Debugging](../../troubleshooting/debugging.md).

## Source paths inspected

- `Source/Frontend/ApplicationInit.cpp`
- `Source/Frontend/Application.h`
- `Source/Essentials/BaseLogging.cpp`
- `Source/Essentials/BaseLogging.h`
- `Source/Essentials/Logging.cpp`
- `Source/Essentials/Logging.h`
- `Source/Essentials/DiskFileSystem.cpp`
- `Source/Applications/ClientApp.cpp`
- `Source/Applications/ClientLib.cpp`
- `Source/Applications/ServerApp.cpp`
- `Source/Applications/BakerLib.cpp`
- `Source/Applications/TestingApp.cpp`
- `Source/Client/ClientSessionMarker.cpp`
- `Source/Server/Server.cpp`
- `Source/Common/Settings.inc`
- `BuildTools/cmake/stages/Applications.cmake`
- `BuildTools/cmake/RunTestExecutable.cmake`
- `Docs/en/explanation/runtime/client-updater.md`
- `Docs/en/contributing/testing/index.md`
- `Docs/en/how-to/release/operations.md`
- `Docs/en/troubleshooting/debugging.md`
- `Docs/en/how-to/platforms/android-debugging.md`
- `Docs/en/how-to/platforms/web-debugging.md`

## File names and ownership

Every application that calls `InitApp` opens one log file during startup, before configuration is read. The name is the executable's file name without its extension, plus `.log`. If the executable path cannot be resolved, the name falls back to `<DevName>.log`, where `<DevName>` is the project's compile-time development name.

The name follows the executable on disk, so renaming a packaged executable renames its log. With the default Engine target names the files are:

| Process | Default log file | Opened | Previous run kept |
|---|---|---|---|
| Client (`<DevName>_Client`, `<DevName>_ClientHeadless`) | `<exe>.log` | Host renames the old file, truncates the new one, then the runtime appends | One previous run in `<exe>.prev.log` |
| Server GUI, headless, daemon and Windows service | `<exe>.log` | Truncated on every start | No |
| Mapper, animation viewer, particle viewer | `<exe>.log` | Truncated on every start | No |
| Baker, AngelScript compiler, managed script baker | `<exe>.log` | Truncated on every start, without timestamp tags | No |
| Resource prebake inside a server or client process | `<DevName>_BakerLib.log` | Truncated at each bake, relative to the working directory | No |
| Unit-test executable | `<DevName>_UnitTests.log` | Truncated on every start | No |

Related files that are not engine logs:

- `<exe>_Health.txt` is the optional server health file, enabled by `Server.WriteHealthFile`. It holds a state word, not log text. See [Release Operations](../../how-to/release/operations.md).
- `<DevName>_Log_<YYYY.MM.DD>_<HH-MM-SS>.log` is written by the **Save log** button of the server GUI. It contains the lines currently held in the on-screen buffer, limited by `Server.MaxServerLogLines`, and is never overwritten or removed by the Engine.
- `<exe>.session` is the client session marker described below.
- `<build-dir>/Testing/<target>-<config>.log` is the captured console output of the `RunUnitTests` target. It is a build-tree file, not an application log.

## Where the file is written

The directory is the **writable root**. The root is resolved from the command line and an installer marker only, so the log can open at its final location before any config file is read:

1. `--Common.UserWritablePath <path>` on the command line names the root. The value `*` selects the per-user data directory.
2. Otherwise, an `INSTALLED` marker file beside the executable selects the per-user data directory plus the project name.
3. Otherwise, the root is empty and the log is written to the **current working directory** of the process.

`Common.UserWritablePath` is read-only and cannot be set from a config file. See [Client Runtime Split and Updater](../../explanation/runtime/client-updater.md) for the per-OS directories. If the requested root cannot be created, the process logs a warning and writes to the working directory instead.

The working directory therefore matters for unattended runs. A service manager, a container entry point or a CI step may start the process from a directory nobody expects. Set the working directory, or pass `--Common.UserWritablePath`, explicitly. The `BakerLib` prebake log ignores the writable root and is always created relative to the working directory.

If the log file cannot be opened, the process reports `Can't create log file` on the console and continues without a file. A missing log is not a startup error.

### Platform differences

- **Web:** file logging is disabled. Use the browser console; see [Web debugging](../../how-to/platforms/web-debugging.md).
- **Android:** the writable root is the app-private storage the launcher passes on the command line; the log is `files/<project>.log` for a debug build. See [Android debugging](../../how-to/platforms/android-debugging.md).
- **Windows, Linux, macOS:** the rules above apply. macOS behavior is unobserved for this reference.

## Rotation and the client session marker

Only the client rotates its log. Before it opens the new file, the client host renames the existing `<client>.log` to `<client>.prev.log` in the writable root, so a crash or hang of the previous run is still readable after a restart. If another process holds the old file open and the rename fails, the host writes a warning and continues; the previous run is then lost. The retained file is exactly one previous run, not an archive. A second restart overwrites it.

The host holds the new file open for the whole launch, and the client runtime opens the same file in append mode. The two halves of one run therefore read as one log. A few runtime lines written before the runtime initializes its own logging go to the console only.

Next to the log, the client keeps `<client>.session`, a small text marker named after the executable:

- The runtime writes it at session start with the stage `Running`, the build hash, the start time and the process identity.
- The stage line is rewritten, not appended, as shutdown advances (main loop exited, client stopped, application reset, shutdown hook done, global data teardown, runtime returned, exit requested). A teardown line names the subsystem set being destroyed.
- At the next launch the marker is read and deleted. If the previous process did not reach `ExitRequested`, or is still running, the new run logs a warning with the stage, teardown set, build and start time, and reports it as a non-fatal exception. This distinguishes a crash, a hang during exit and a second running client.
- A marker that cannot be parsed is consumed and ignored.

The marker is a diagnostic aid. Keep it with the `.prev.log` when collecting evidence from a client that exited abnormally, and do not delete it by hand before the next launch.

## Content and format

- Each line is `[dd/mm/yy] [hh:mm:ss] <message>`. Messages from threads other than the main thread add the thread name as a third tag. Tools started with the tag-disable flag (baker, AngelScript compiler, managed script baker) write the message only.
- The message type (info, warning, error) is **not** written into the line. It is passed to log callbacks only. Search the text, not a level prefix.
- Identical consecutive messages are collapsed. The next different message is preceded by `...and N more same messages`.
- Every line is also written to standard output. A supervisor that captures stdout therefore receives a second copy of the log.
- Fatal and crash paths write synchronously, including a stack trace with script and native frames. See [Debugging](../../troubleshooting/debugging.md).
- Command-line setting overrides whose names contain a secret token (`Common.SecretSettingTokens`) are masked in the log. This is the only masking. See [Security and Secrets](../../how-to/release/security-and-secrets.md).

## Levels and settings

The Engine has **no log level setting** and no per-category filter. Everything that is written is kept. The following settings affect logging:

| Setting | Effect |
|---|---|
| `Common.AsyncLogWrite` | Writes to the file and console from a background thread. The queue holds at most 100000 messages; beyond that messages are dropped and a `Dropped N log messages due to high volume` notice is written. Fatal errors, failed strong assertions and crash reports switch the logger back to synchronous writing before they report. |
| `Common.NonInteractive` | Error dialogs log the error and request a failed exit instead of waiting for input. |
| `Common.SecretSettingTokens` | Name substrings whose command-line values are masked. |
| `Network.MoveSyncTrace` | Adds one `MOVESYNC` line per movement synchronization event on server and client; too verbose for production. |
| `ManagedScript.InteropProbeOnStart` | Adds one `INTEROP-TRANSPORT` line per checked transport at script start. |
| `Server.MaxServerLogLines`, `Server.CollapseLogOnStart` | Affect only the server GUI log view and its **Save log** button. |

See the [settings reference](../script-api/settings.md) for current defaults. Projects that need levels or structured output attach their own sink with the logging callback mechanism and filter there.

## Retention: what exists and what does not

| Behavior | Present |
|---|---|
| Truncate on start (non-client applications) | Yes |
| One previous run for the client | Yes, `<client>.prev.log` |
| Size-based rotation | No |
| Age-based deletion | No |
| Compression or archiving | No |
| Per-day or per-run file names | No (except the manual server GUI export) |

Two consequences follow. A server restart overwrites the previous server log, so the evidence of a crash is lost unless it is copied first. A long-running server log grows without bound.

The file is opened once, stays open for the life of the process, and each write seeks to the end first. Consequently a copy-then-truncate rotation tool keeps working, while a rename-based rotation leaves the process writing into the renamed file. This follows from the write path in `BaseLogging.cpp`; it was derived from source and not exercised against a specific rotation tool.

## Recommended project practice

1. **Choose the root deliberately.** For a server, pass `--Common.UserWritablePath <writable-root>` and set the supervisor's working directory to the same location. The log, health file, cache and database then share one directory that can be sized, backed up or cleaned as a unit. Keep it outside the immutable release unit so a rollback does not discard logs.
2. **Archive before restart.** A deploy script or supervisor should copy `<exe>.log` to a timestamped name before it stops or restarts the server, then keep a bounded number of copies. Preserve the log with the release identity when a rollout fails; [Release Operations](../../how-to/release/operations.md) requires preserving logs before replacing evidence.
3. **Rotate externally.** For a long-running server use a copy-then-truncate rotation with size and age limits that the project chooses. The Engine does not enforce them.
4. **Collect client evidence as a set.** `<client>.log`, `<client>.prev.log` and `<client>.session` together explain a crash or a hang during exit.
5. **Ignore generated logs in version control.** Logs written to the working directory can land in the repository root, especially from unit-test and prebake runs.

```text
# Engine runtime and tool logs
*.log
*.prev.log
*.session
*_Health.txt
```

Narrow the patterns if the project tracks other `.log` files.

6. **Treat logs as sensitive.** Production logs and crash attachments need an independent redaction review; the Engine masks only the command-line override case. Do not upload a whole writable root as a CI artifact; collect named files. See [CI trust boundaries](../../how-to/release/security-and-secrets.md#ci-trust-boundaries) and [Set Up Continuous Integration for a Game Project](../../how-to/project/continuous-integration.md).

## Unit-test logs

The unit-test executable is an ordinary Engine application and writes `<DevName>_UnitTests.log` like any other, relative to its working directory. The `RunUnitTests` target runs it with the project source directory as the working directory and writes the process output to `<build-dir>/Testing/<target>-<config>.log`; a failing run echoes that output to the build console. Retain the build-tree log for CI evidence and ignore the working-directory file. See [Testing](../../contributing/testing/index.md).

## Validation checklist

1. The log of each deployed process appears in the intended writable root, not in an unexpected working directory.
2. A server restart procedure copies the previous log before the new process truncates it.
3. A client launched twice leaves `<client>.prev.log` holding the first run.
4. A forced kill of a client produces a session warning at the next launch.
5. The retention policy, including limits and who enforces them, is recorded by the project, because the Engine enforces none.
6. Generated logs are ignored by version control and excluded from release packages.
