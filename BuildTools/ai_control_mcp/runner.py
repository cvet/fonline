"""MCP client primitives for project smoke tests and playtest runners that drive the adapter.

A runner starts the adapter as a child process, speaks MCP over its stdio and calls tools. Project
scenario runners build on these helpers instead of redefining the client loop.
"""

from __future__ import annotations

import json
import os
import queue
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Any

MCP_PROTOCOL_VERSION = "2025-11-25"
ADAPTER_ENTRY = Path(__file__).resolve().parent
DEFAULT_REQUEST_TIMEOUT_SECONDS = 420.0
# At end of input the adapter waits for every owned process tree to stop
CLOSE_EXIT_GRACE_SECONDS = 10.0
REPORT_WRITE_RETRY_ATTEMPTS = 20
REPORT_WRITE_RETRY_DELAY_SECONDS = 0.05
READINESS_LOG_MARKERS = ("Exception", "FATAL", "ASSERT", "error", "Error", "failed", "Failed", "refused", "timeout")


class McpRunnerError(RuntimeError):
    pass


def configure_stdio() -> None:
    """Use UTF-8 and line buffering so a killed long run still leaves the progress it printed."""
    for stream in (sys.stdin, sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)

        if reconfigure is None:
            continue

        try:
            reconfigure(encoding="utf-8", errors="replace", line_buffering=True)
        except (OSError, ValueError, TypeError):
            reconfigure(encoding="utf-8", errors="replace")


def build_adapter_command(arguments: list[str], *, python: str | None = None) -> list[str]:
    """Command that starts the Engine adapter; tokens travel through --token-env, never as arguments."""
    for argument in arguments:
        if argument == "--token" or argument.startswith("--token="):
            raise McpRunnerError("pass the bridge token through the environment variable named by --token-env")

    return [python or sys.executable, str(ADAPTER_ENTRY), *arguments]


class McpProcess:
    """An MCP client over the stdio of an adapter child process."""

    def __init__(
        self,
        command: list[str],
        request_timeout: float = DEFAULT_REQUEST_TIMEOUT_SECONDS,
        env: dict[str, str] | None = None,
        cwd: str | None = None,
    ) -> None:
        self._next_id = 1
        self._request_timeout = request_timeout
        self._abandoned_request_ids: set[int] = set()
        self._process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            bufsize=1,
            env=env,
            cwd=cwd,
        )
        self._stdout_lines: queue.Queue[str | None] = queue.Queue()
        self._stderr_lines: queue.Queue[str | None] = queue.Queue()
        self._stdout_thread = threading.Thread(target=_read_pipe, args=(self._process.stdout, self._stdout_lines), daemon=True)
        self._stderr_thread = threading.Thread(target=_read_pipe, args=(self._process.stderr, self._stderr_lines), daemon=True)
        self._stdout_thread.start()
        self._stderr_thread.start()

    @property
    def process(self) -> subprocess.Popen[str]:
        return self._process

    def request(self, method: str, params: dict[str, Any] | None = None, timeout: float | None = None) -> dict[str, Any]:
        request_id = self._next_id
        self._next_id += 1
        self._write({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params or {}})
        request_timeout = self._request_timeout if timeout is None else max(timeout, 0.001)
        deadline = time.monotonic() + request_timeout

        while True:
            remaining = deadline - time.monotonic()

            try:
                if remaining <= 0.0:
                    raise queue.Empty
                line = self._stdout_lines.get(timeout=remaining)
            except queue.Empty:
                if self._process.poll() is not None:
                    raise McpRunnerError(f"adapter exited with code {self._process.returncode} while waiting for {method}")

                # A late answer to this id must not be taken for the response to a later request
                self._abandoned_request_ids.add(request_id)
                raise McpRunnerError(f"adapter response timeout after {request_timeout:.1f}s for {method}")

            if not line:
                raise McpRunnerError("adapter closed stdout before returning a response")

            response = json.loads(line)
            response_id = response.get("id")

            if response_id is None and "method" in response:
                continue

            if response_id != request_id:
                if response_id in self._abandoned_request_ids:
                    self._abandoned_request_ids.remove(response_id)
                    continue

                raise McpRunnerError(f"unexpected MCP response id: {response_id} != {request_id}")

            return response

    def notify(self, method: str, params: dict[str, Any] | None = None) -> None:
        self._write({"jsonrpc": "2.0", "method": method, "params": params or {}})

    def stderr_text(self) -> str:
        lines: list[str] = []

        while True:
            try:
                line = self._stderr_lines.get_nowait()
            except queue.Empty:
                break

            if line:
                lines.append(line)

        return "".join(lines)

    def close(self) -> int:
        if self._process.stdin is not None:
            try:
                self._process.stdin.close()
            except BrokenPipeError:
                pass

        try:
            self._process.wait(timeout=CLOSE_EXIT_GRACE_SECONDS)
        except subprocess.TimeoutExpired:
            self._process.terminate()

            try:
                self._process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                self._process.kill()
                self._process.wait(timeout=2.0)

        for thread, pipe in ((self._stdout_thread, self._process.stdout), (self._stderr_thread, self._process.stderr)):
            thread.join(timeout=2.0)

            if pipe is not None and not thread.is_alive():
                pipe.close()

        return int(self._process.returncode)

    def _write(self, message: dict[str, Any]) -> None:
        if self._process.poll() is not None:
            raise McpRunnerError(f"adapter exited with code {self._process.returncode}")
        if self._process.stdin is None:
            raise McpRunnerError("adapter stdin is closed")

        self._process.stdin.write(json.dumps(message, ensure_ascii=False, separators=(",", ":")) + "\n")
        self._process.stdin.flush()


def _read_pipe(pipe: Any, lines: queue.Queue[str | None]) -> None:
    if pipe is None:
        lines.put(None)
        return

    try:
        for line in pipe:
            lines.put(line)
    finally:
        lines.put(None)


def require_result(response: dict[str, Any], label: str) -> dict[str, Any]:
    if "error" in response:
        raise McpRunnerError(f"{label} failed: {json.dumps(response['error'], ensure_ascii=False)}")

    result = response.get("result")

    if not isinstance(result, dict):
        raise McpRunnerError(f"{label} returned no result object")

    return result


def initialize_client(client: McpProcess, client_name: str, version: str = "0.1.0") -> dict[str, Any]:
    result = require_result(
        client.request(
            "initialize",
            {"protocolVersion": MCP_PROTOCOL_VERSION, "capabilities": {}, "clientInfo": {"name": client_name, "version": version}},
        ),
        "initialize",
    )
    client.notify("notifications/initialized")
    return result


def call_tool(
    client: McpProcess, name: str, arguments: dict[str, Any] | None = None, request_timeout: float | None = None
) -> dict[str, Any]:
    """Call a tool and return its JSON object payload; a tool error raises McpRunnerError."""
    response = client.request("tools/call", {"name": name, "arguments": arguments or {}}, timeout=request_timeout)
    result = require_result(response, name)
    content = result.get("content")

    if not isinstance(content, list) or not content or not isinstance(content[0], dict):
        raise McpRunnerError(f"{name} returned no content")

    text = content[0].get("text")

    if not isinstance(text, str):
        raise McpRunnerError(f"{name} returned non-text content")

    payload = json.loads(text)

    if result.get("isError"):
        raise McpRunnerError(f"{name} tool error: {json.dumps(payload, ensure_ascii=False)}")
    if not isinstance(payload, dict):
        raise McpRunnerError(f"{name} returned a non-object payload")

    return payload


def endpoint_args(endpoint: dict[str, Any]) -> dict[str, Any]:
    return {"endpointId": str(endpoint["endpointId"])}


def command_args(
    endpoint: dict[str, Any],
    command: dict[str, Any],
    *,
    wait: bool = True,
    timeout_ms: int = 20000,
    poll_interval_ms: int = 150,
    sync_after_completion: bool | None = None,
    include_observation: bool = False,
) -> dict[str, Any]:
    return {
        **endpoint_args(endpoint),
        "command": dict(command),
        "waitForCompletion": wait,
        "timeoutMs": timeout_ms,
        "pollIntervalMs": poll_interval_ms,
        "syncAfterCompletion": wait if sync_after_completion is None else sync_after_completion,
        "includeObservation": include_observation,
    }


def verify_command_payload(payload: dict[str, Any], label: str) -> None:
    """Raise unless an act payload shows acceptance and, when waited for, a successful fresh completion."""
    if payload.get("accepted") is not True:
        raise McpRunnerError(f"{label} was not accepted: {json.dumps(payload, ensure_ascii=False)}")

    completion = payload.get("completion")

    if not isinstance(completion, dict):
        return

    if not completion.get("completed"):
        reason = "the bridge event history restarted" if completion.get("historyReset") else "timeout"
        raise McpRunnerError(f"{label} did not complete: {reason}")

    event = completion.get("event") if isinstance(completion.get("event"), dict) else {}

    if event.get("success") is False:
        raise McpRunnerError(f"{label} failed: {event.get('message')}")


def wait_ready(
    client: McpProcess,
    endpoint: dict[str, Any],
    *,
    tool_prefix: str = "fo_",
    timeout_ms: int = 60000,
    require_fields: list[str] | None = None,
) -> dict[str, Any]:
    arguments: dict[str, Any] = {**endpoint_args(endpoint), "timeoutMs": timeout_ms}

    if require_fields is not None:
        arguments["requireFields"] = list(require_fields)

    ready = call_tool(client, tool_prefix + "wait_ready", arguments, request_timeout=timeout_ms / 1000.0 + 30.0)

    if not ready.get("ready"):
        raise McpRunnerError(f"endpoint not ready: {ready.get('missing')}{readiness_diagnosis(client, timeout_ms, tool_prefix)}")

    return ready


def readiness_diagnosis(client: McpProcess, timeout_ms: int, tool_prefix: str = "fo_") -> str:
    """Owned processes and the error lines of their logs, collected when a readiness wait fails."""
    parts: list[str] = [f" (waited {timeout_ms} ms)"]

    try:
        processes = call_tool(client, tool_prefix + "processes")["processes"]
    except McpRunnerError as error:
        return parts[0] + f"; process list unavailable: {error}"

    parts.append("processes: " + (", ".join(
        f"{item.get('role')}#{item.get('processId')}{'' if item.get('running') else ' (exited ' + str(item.get('returnCode')) + ')'}"
        for item in processes
    ) or "none"))

    for item in processes:
        try:
            text = str(call_tool(client, tool_prefix + "logs", {"processId": item.get("processId"), "lines": 200}).get("text", ""))
        except McpRunnerError:
            continue

        hits = [line.strip()[:160] for line in text.splitlines() if any(marker in line for marker in READINESS_LOG_MARKERS)]

        if hits:
            parts.append(f"process {item.get('processId')} log: " + " | ".join(hits[-3:]))

    return "; ".join(parts)


def write_json_report(path: Path, report: dict[str, Any]) -> None:
    write_text_report(path, json.dumps(report, ensure_ascii=False, indent=2) + "\n")


def write_text_report(path: Path, text: str) -> None:
    """Replace the report atomically, retrying while a reader briefly holds the file open on Windows."""
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    temporary_path.write_text(text, encoding="utf-8")

    try:
        for attempt in range(REPORT_WRITE_RETRY_ATTEMPTS):
            try:
                os.replace(temporary_path, path)
                return
            except PermissionError:
                if attempt + 1 >= REPORT_WRITE_RETRY_ATTEMPTS:
                    raise

                time.sleep(REPORT_WRITE_RETRY_DELAY_SECONDS)
    finally:
        temporary_path.unlink(missing_ok=True)
