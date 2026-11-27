"""Independent owner for process trees launched by the AiControl MCP adapter.

The adapter never stops a process by image name. Each launched program runs under its own guardian
process: on Windows the guardian puts itself and every descendant into a kill-on-close job object;
on Linux it becomes a child subreaper and stops only the descendants it adopted. Losing the adapter
(EOF, signal or hard death) closes the guardian's control pipe and the guardian stops its tree.
"""

from __future__ import annotations

import ctypes
import json
import os
import queue
import signal
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Any


OWNER_START_TIMEOUT_SECONDS = 15.0
OWNER_STOP_GRACE_SECONDS = 1.0


class ProcessOwnershipError(RuntimeError):
    pass


def spawn_managed_process(
    command: list[str], cwd: str, keep_processes_on_exit: bool, env: dict[str, str] | None = None
) -> Any:
    if keep_processes_on_exit:
        return subprocess.Popen(
            command,
            cwd=cwd,
            env=env,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

    return OwnedProcess(command, cwd, env)


def guardian_command() -> list[str]:
    return [sys.executable, "-I", "-u", str(Path(__file__).resolve())]


class OwnedProcess:
    """Popen-like view of the target process; the guardian's control pipe belongs to the adapter."""

    def __init__(self, command: list[str], cwd: str, env: dict[str, str] | None = None) -> None:
        self.returncode: int | None = None
        self._child_returncode: int | None = None
        self._events: queue.Queue[Any] = queue.Queue()
        self._guardian = subprocess.Popen(
            guardian_command(),
            cwd=cwd,
            env=env,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=None,
            text=True,
            encoding="utf-8",
            bufsize=1,
            start_new_session=os.name == "posix",
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == "win32" else 0,
        )
        self._reader = threading.Thread(target=self._read_status, daemon=True)
        self._reader.start()

        try:
            self._send({"command": command, "cwd": cwd})
            event = self._events.get(timeout=OWNER_START_TIMEOUT_SECONDS)

            if not isinstance(event, dict) or "pid" not in event:
                raise ProcessOwnershipError(f"Process ownership setup failed: {event}")

            self.pid = int(event["pid"])
        except BaseException:
            # Includes a signal delivered between owner creation and registration
            self._close_control()
            self.wait(timeout=OWNER_START_TIMEOUT_SECONDS)
            raise

    def poll(self) -> int | None:
        if self.returncode is not None:
            return self.returncode

        if self._guardian.poll() is None:
            return None

        self._reader.join(timeout=1.0)
        self._collect_exit_status()
        self._close_pipes()
        return self.returncode

    def wait(self, timeout: float | None = None) -> int:
        self._guardian.wait(timeout=timeout)
        self._reader.join(timeout=1.0)
        self._collect_exit_status()
        self._close_pipes()
        assert self.returncode is not None
        return self.returncode

    def terminate(self) -> None:
        self._send({"stop": "terminate"})

    def kill(self) -> None:
        # The guardian itself must outlive the stop so it can reap adopted descendants on Linux
        self._send({"stop": "kill"})

    def _send(self, message: dict[str, Any]) -> None:
        pipe = self._guardian.stdin

        if pipe is None or pipe.closed:
            return

        try:
            pipe.write(json.dumps(message) + "\n")
            pipe.flush()
        except BrokenPipeError:
            self._close_control()

    def _read_status(self) -> None:
        assert self._guardian.stdout is not None

        try:
            for line in self._guardian.stdout:
                self._events.put(json.loads(line))
        except (OSError, ValueError) as exc:
            self._events.put({"error": str(exc)})
        finally:
            self._events.put(None)

    def _collect_exit_status(self) -> None:
        while True:
            try:
                event = self._events.get_nowait()
            except queue.Empty:
                break

            if isinstance(event, dict) and "returnCode" in event:
                self._child_returncode = int(event["returnCode"])
            elif isinstance(event, dict) and "error" in event:
                sys.stderr.write(f"Process owner failed: {event['error']}\n")

        self.returncode = self._child_returncode

        if self.returncode is None:
            self.returncode = self._guardian.returncode or 1

    def _close_control(self) -> None:
        pipe = self._guardian.stdin

        if pipe is not None and not pipe.closed:
            try:
                pipe.close()
            except BrokenPipeError:
                pass

    def _close_pipes(self) -> None:
        self._close_control()

        if self._guardian.stdout is not None and not self._reader.is_alive():
            self._guardian.stdout.close()


def guardian_main() -> int:
    process: subprocess.Popen[Any] | None = None
    job: Any = None
    force = False

    try:
        if sys.platform == "win32":
            job = arm_windows_owner()
        elif sys.platform.startswith("linux"):
            arm_linux_owner()
        else:
            raise ProcessOwnershipError("Process ownership requires Windows or Linux; use --keep-processes-on-exit")

        control = bytearray()

        while b"\n" not in control:
            chunk = os.read(sys.stdin.fileno(), 4096)

            if not chunk:
                return 0

            control.extend(chunk)

        line, _, remainder = control.partition(b"\n")
        request = json.loads(line)
        process = subprocess.Popen(
            request["command"],
            cwd=request["cwd"],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        report_status({"pid": process.pid})
        force = watch_owner(process, bytearray(remainder))
    except Exception as exc:  # noqa: BLE001 - the guardian reports every setup failure to its adapter
        report_status({"error": str(exc)})
    finally:
        if process is not None:
            if sys.platform.startswith("linux"):
                stop_linux_tree(process, force)
            elif process.poll() is None:
                stop_primary_process(process, force)

            report_status({"returnCode": process.returncode})

        if job is not None:
            # The guardian is inside the job too; its final status line is already written
            windows_kernel32().CloseHandle(job)

    return 0


def report_status(status: dict[str, Any]) -> None:
    payload = json.dumps(status).encode("utf-8") + b"\n"

    try:
        # Unbuffered writes: a dead reader must not leave a pending flush for interpreter exit
        while payload:
            written = os.write(sys.stdout.fileno(), payload)
            payload = payload[written:]
    except BrokenPipeError:
        # The adapter is gone; the guardian still has to finish cleaning up its tree
        pass


def watch_owner(process: subprocess.Popen[Any], control: bytearray) -> bool:
    # A reader thread is required on Windows, where select() does not work on anonymous pipes
    messages: queue.Queue[bytes] = queue.Queue()

    def read_control() -> None:
        if control:
            messages.put(bytes(control))

        while True:
            chunk = os.read(sys.stdin.fileno(), 4096)
            messages.put(chunk)

            if not chunk:
                return

    threading.Thread(target=read_control, daemon=True).start()
    pending = bytearray()

    while process.poll() is None:
        try:
            chunk = messages.get(timeout=0.05)
        except queue.Empty:
            continue

        if not chunk:
            return False

        pending.extend(chunk)

        while b"\n" in pending:
            line, _, pending = pending.partition(b"\n")
            message = json.loads(line)

            if message.get("stop") in {"terminate", "kill"}:
                return message["stop"] == "kill"

    return False


def stop_primary_process(process: subprocess.Popen[Any], force: bool) -> None:
    if force:
        process.kill()
    else:
        process.terminate()

    try:
        process.wait(timeout=OWNER_STOP_GRACE_SECONDS)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def arm_linux_owner() -> None:
    if not hasattr(os, "pidfd_open") or not hasattr(signal, "pidfd_send_signal"):
        raise ProcessOwnershipError("Process ownership requires Linux pidfd support")

    descriptor = os.pidfd_open(os.getpid())

    try:
        signal.pidfd_send_signal(descriptor, 0)
    finally:
        os.close(descriptor)

    Path(f"/proc/self/task/{os.getpid()}/children").read_text()
    libc = ctypes.CDLL(None, use_errno=True)
    libc.prctl.argtypes = [ctypes.c_int, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong, ctypes.c_ulong]
    libc.prctl.restype = ctypes.c_int

    if libc.prctl(36, 1, 0, 0, 0) != 0:  # PR_SET_CHILD_SUBREAPER
        raise OSError(ctypes.get_errno(), "Cannot arm child subreaper")


def stop_linux_tree(process: subprocess.Popen[Any], force: bool) -> None:
    if process.poll() is None:
        stop_primary_process(process, force)

    children_path = Path(f"/proc/self/task/{os.getpid()}/children")

    while True:
        children = [int(pid) for pid in children_path.read_text().split()]

        if not children:
            return

        for pid in children:
            try:
                descriptor = os.pidfd_open(pid)
            except ProcessLookupError:
                continue

            try:
                # Only not-yet-reaped children of this live guardian; the pidfd pins the target
                signal.pidfd_send_signal(descriptor, signal.SIGKILL)
            except ProcessLookupError:
                pass
            finally:
                os.close(descriptor)

        while True:
            try:
                pid, _ = os.waitpid(-1, os.WNOHANG)
            except ChildProcessError:
                break

            if pid == 0:
                break

        time.sleep(0.01)


def windows_kernel32() -> Any:
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateJobObjectW.restype = wintypes.HANDLE
    kernel32.CreateJobObjectW.argtypes = [wintypes.LPVOID, wintypes.LPCWSTR]
    kernel32.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, wintypes.LPVOID, wintypes.DWORD]
    kernel32.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
    kernel32.GetCurrentProcess.restype = wintypes.HANDLE
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    return kernel32


def arm_windows_owner() -> Any:
    from ctypes import wintypes

    class IoCounters(ctypes.Structure):
        _fields_ = [
            (name, ctypes.c_ulonglong)
            for name in (
                "ReadOperationCount",
                "WriteOperationCount",
                "OtherOperationCount",
                "ReadTransferCount",
                "WriteTransferCount",
                "OtherTransferCount",
            )
        ]

    class BasicLimits(ctypes.Structure):
        _fields_ = [
            ("PerProcessUserTimeLimit", ctypes.c_longlong),
            ("PerJobUserTimeLimit", ctypes.c_longlong),
            ("LimitFlags", wintypes.DWORD),
            ("MinimumWorkingSetSize", ctypes.c_size_t),
            ("MaximumWorkingSetSize", ctypes.c_size_t),
            ("ActiveProcessLimit", wintypes.DWORD),
            ("Affinity", ctypes.c_size_t),
            ("PriorityClass", wintypes.DWORD),
            ("SchedulingClass", wintypes.DWORD),
        ]

    class ExtendedLimits(ctypes.Structure):
        _fields_ = [
            ("BasicLimitInformation", BasicLimits),
            ("IoInfo", IoCounters),
            ("ProcessMemoryLimit", ctypes.c_size_t),
            ("JobMemoryLimit", ctypes.c_size_t),
            ("PeakProcessMemoryUsed", ctypes.c_size_t),
            ("PeakJobMemoryUsed", ctypes.c_size_t),
        ]

    kernel32 = windows_kernel32()
    job = kernel32.CreateJobObjectW(None, None)

    if not job:
        raise ctypes.WinError(ctypes.get_last_error())

    try:
        info = ExtendedLimits()
        info.BasicLimitInformation.LimitFlags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE

        if not kernel32.SetInformationJobObject(job, 9, ctypes.byref(info), ctypes.sizeof(info)):
            raise ctypes.WinError(ctypes.get_last_error())

        if not kernel32.AssignProcessToJobObject(job, kernel32.GetCurrentProcess()):
            raise ctypes.WinError(ctypes.get_last_error())

        return job
    except BaseException:
        kernel32.CloseHandle(job)
        raise


if __name__ == "__main__":
    raise SystemExit(guardian_main())
