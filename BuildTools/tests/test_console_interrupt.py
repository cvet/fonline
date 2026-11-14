from __future__ import annotations

import ctypes
import os
from pathlib import Path
import subprocess
import sys
import time

import pytest


@pytest.mark.skipif(sys.platform != "win32", reason="Windows console control events")
@pytest.mark.parametrize("event", [0, 1], ids=["ctrl-c", "ctrl-break"])
@pytest.mark.parametrize("behavior", ["application-loop", "deferred"], ids=["application-loop", "deferred"])
def test_console_interrupt_respects_application_policy(tmp_path: Path, event: int, behavior: str) -> None:
    executable = os.environ.get("FONLINE_TEST_EXECUTABLE")
    if not executable:
        pytest.skip("Set FONLINE_TEST_EXECUTABLE to the generated UnitTests executable")
    executable_path = Path(executable).resolve(strict=True)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    # A private console isolates Ctrl+C, which cannot target a single process group
    worker = subprocess.Popen(
        [sys.executable, str(Path(__file__).resolve()), "--console-worker", str(executable_path), str(tmp_path), str(event), behavior],
        creationflags=subprocess.CREATE_NEW_CONSOLE,
        startupinfo=startup,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    try:
        stdout, stderr = worker.communicate(timeout=40)
    except subprocess.TimeoutExpired:
        subprocess.run(["taskkill", "/PID", str(worker.pid), "/T", "/F"], capture_output=True, timeout=10, check=False)
        worker.communicate(timeout=10)
        pytest.fail("Console worker timed out; its owned process tree was stopped")
    child_log = tmp_path / "child.log"
    output = child_log.read_text(encoding="utf-8", errors="replace") if child_log.exists() else ""
    assert worker.returncode == 0, stdout + stderr + output
    if behavior == "application-loop":
        completion = tmp_path / "console-interrupt.quit"
    else:
        completion = tmp_path / "console-interrupt-deferred.quit"
    assert completion.read_text(encoding="utf-8") == "quit"


def _console_worker(executable: str, work: Path, event: int, behavior: str) -> None:
    from ctypes import wintypes

    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    handler_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.DWORD)
    kernel.SetConsoleCtrlHandler.argtypes = [handler_type, wintypes.BOOL]
    kernel.SetConsoleCtrlHandler.restype = wintypes.BOOL
    kernel.GenerateConsoleCtrlEvent.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel.GenerateConsoleCtrlEvent.restype = wintypes.BOOL
    handler = handler_type(lambda received: received in (0, 1))
    if not kernel.SetConsoleCtrlHandler(handler_type(), False):
        raise ctypes.WinError(ctypes.get_last_error())
    if not kernel.SetConsoleCtrlHandler(handler, True):
        raise ctypes.WinError(ctypes.get_last_error())

    if behavior == "application-loop":
        test_filter = "ApplicationConsoleInterrupt"
        ready_marker = work / "console-interrupt.ready"
        release_marker = None
        completion_marker = work / "console-interrupt.quit"
    else:
        test_filter = "ApplicationConsoleInterruptDeferred"
        ready_marker = work / "console-interrupt-deferred.ready"
        release_marker = work / "console-interrupt-deferred.release"
        completion_marker = work / "console-interrupt-deferred.quit"

    with (work / "child.log").open("w", encoding="utf-8") as log:
        child = subprocess.Popen([executable, test_filter], cwd=work, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 15
            while not ready_marker.exists():
                if child.poll() is not None or time.monotonic() >= deadline:
                    raise AssertionError("Application did not reach the console-interrupt wait")
                time.sleep(0.02)

            if not kernel.GenerateConsoleCtrlEvent(event, 0):
                raise ctypes.WinError(ctypes.get_last_error())

            if behavior == "deferred":
                # The control event must only latch the quit flag: the process must stay alive
                # until the release marker is written
                alive_deadline = time.monotonic() + 2
                while time.monotonic() < alive_deadline:
                    if child.poll() is not None:
                        raise AssertionError("Application terminated before release marker")
                    time.sleep(0.05)
                release_marker.write_text("release", encoding="utf-8")

            result = child.wait(timeout=10)
            if result != 0:
                raise AssertionError(f"Application exited with {result:#x} after control event {event}")
            if not completion_marker.exists():
                raise AssertionError(f"Completion marker {completion_marker.name} was not written")
        finally:
            if child.poll() is None:
                child.kill()
                child.wait(timeout=5)


if __name__ == "__main__" and len(sys.argv) == 6 and sys.argv[1] == "--console-worker":
    _console_worker(sys.argv[2], Path(sys.argv[3]), int(sys.argv[4]), sys.argv[5])
