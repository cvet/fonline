from __future__ import annotations

import argparse
import json
import os
import socketserver
import sys
import threading
import time
from collections import deque
from pathlib import Path
from typing import Any


ENGINE_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))

from ai_control_client import (  # noqa: E402
    DEFAULT_HOST,
    DEFAULT_PORT,
    JSONRPC_VERSION,
    MAX_LINE_BYTES,
    is_loopback_host,
)


ERROR_PARSE = -32700
ERROR_INVALID_REQUEST = -32600
ERROR_METHOD_NOT_FOUND = -32601
ERROR_INVALID_PARAMS = -32602
ERROR_UNAUTHORIZED = -32001
ERROR_QUEUE_FULL = -32002


class SampleState:
    def __init__(self, token: str, max_commands: int, max_events: int) -> None:
        self.token = token
        self.max_commands = max_commands
        self.max_events = max_events
        self.host = ""
        self.port = 0
        self.last_error = ""
        self.observation_seq = 1
        self.event_seq = 0
        self.command_seq = 0
        self.observation: dict[str, object] = {
            "schemaVersion": 1,
            "ready": True,
            "agent": {"x": 0, "y": 0},
            "availableActions": ["echo", "move", "fail"],
        }
        self.commands: deque[tuple[int, dict[str, object]]] = deque()
        self.events: deque[dict[str, object]] = deque(maxlen=max_events)
        self.lock = threading.Lock()
        self.stop = threading.Event()

    def enqueue(self, command: dict[str, object]) -> int | None:
        with self.lock:
            if len(self.commands) >= self.max_commands:
                return None
            self.command_seq += 1
            self.commands.append((self.command_seq, command))
            return self.command_seq

    def status(self) -> dict[str, object]:
        with self.lock:
            return {
                "running": not self.stop.is_set(),
                "host": self.host,
                "port": self.port,
                "queuedCommands": len(self.commands),
                "maxQueuedCommands": self.max_commands,
                "events": len(self.events),
                "maxEvents": self.max_events,
                "observationSeq": self.observation_seq,
                "lastError": self.last_error,
            }

    def observe(self) -> dict[str, object]:
        with self.lock:
            return {
                "observationSeq": self.observation_seq,
                "observation": json.loads(json.dumps(self.observation)),
            }

    def events_after(self, after_seq: int, limit: int) -> dict[str, object]:
        with self.lock:
            selected = [
                event for event in self.events if int(event["seq"]) > after_seq
            ][:limit]
            return {"latestSeq": self.event_seq, "events": selected}

    def process_one(self) -> bool:
        with self.lock:
            if not self.commands:
                return False
            command_seq, command = self.commands.popleft()
            command_type = str(command["type"])
            success = True
            message = "completed"
            if command_type == "move":
                agent = self.observation["agent"]
                assert isinstance(agent, dict)
                agent["x"] = int(command.get("x", agent["x"]))
                agent["y"] = int(command.get("y", agent["y"]))
                self.observation_seq += 1
                message = "moved"
            elif command_type == "echo":
                message = str(command.get("stringArg", ""))
            elif command_type == "fail":
                success = False
                message = "sample_failure"
            else:
                success = False
                message = "unknown_sample_command"
            self.event_seq += 1
            self.events.append(
                {
                    "seq": self.event_seq,
                    "event": {
                        "type": "command_completed",
                        "commandSeq": command_seq,
                        "success": success,
                        "message": message,
                    },
                }
            )
            return True


def _response(request_id: object, result: object) -> dict[str, object]:
    return {"jsonrpc": JSONRPC_VERSION, "id": request_id, "result": result}


def _error(request_id: object, code: int, message: str) -> dict[str, object]:
    return {
        "jsonrpc": JSONRPC_VERSION,
        "id": request_id,
        "error": {"code": code, "message": message},
    }


class AiControlHandler(socketserver.StreamRequestHandler):
    server: Any

    def handle(self) -> None:
        authorized = not bool(self.server.state.token)
        while not self.server.state.stop.is_set():
            line = self.rfile.readline(MAX_LINE_BYTES + 2)
            if not line:
                return
            if not line.endswith(b"\n") or len(line) - 1 > MAX_LINE_BYTES:
                self._write(_error(None, ERROR_INVALID_REQUEST, "Request too large"))
                return
            request_id: object = None
            try:
                request = json.loads(line[:-1].decode("utf-8"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                self._write(_error(None, ERROR_PARSE, "Parse error"))
                continue
            if not isinstance(request, dict):
                self._write(_error(None, ERROR_INVALID_REQUEST, "Invalid request"))
                continue
            request_id = request.get("id")
            method = request.get("method")
            params = request.get("params", {})
            if (
                request.get("jsonrpc") != JSONRPC_VERSION
                or not isinstance(method, str)
                or not method
                or not isinstance(params, dict)
            ):
                self._write(_error(request_id, ERROR_INVALID_REQUEST, "Invalid request"))
                continue
            if method == "auth":
                supplied = params.get("token", "")
                authorized = not self.server.state.token or supplied == self.server.state.token
                self._write(_response(request_id, {"authorized": authorized}))
                continue
            if not authorized:
                self._write(_error(request_id, ERROR_UNAUTHORIZED, "Unauthorized"))
                continue
            if method == "ping":
                self._write(_response(request_id, {"ok": True}))
            elif method == "status":
                self._write(_response(request_id, self.server.state.status()))
            elif method == "observe":
                self._write(_response(request_id, self.server.state.observe()))
            elif method == "events":
                after_seq = params.get("afterSeq", 0)
                limit = params.get("limit", 100)
                if not isinstance(after_seq, int) or not isinstance(limit, int):
                    self._write(_error(request_id, ERROR_INVALID_PARAMS, "Invalid event cursor"))
                    continue
                after_seq = max(after_seq, 0)
                limit = min(max(limit, 1), 500)
                self._write(
                    _response(
                        request_id,
                        self.server.state.events_after(after_seq, limit),
                    )
                )
            elif method == "act":
                command_type = params.get("type")
                if not isinstance(command_type, str) or not command_type:
                    self._write(_error(request_id, ERROR_INVALID_PARAMS, "Missing command type"))
                    continue
                command_seq = self.server.state.enqueue(dict(params))
                if command_seq is None:
                    self._write(_error(request_id, ERROR_QUEUE_FULL, "Command queue full"))
                    continue
                self._write(
                    _response(
                        request_id,
                        {"accepted": True, "commandSeq": command_seq},
                    )
                )
            else:
                self._write(_error(request_id, ERROR_METHOD_NOT_FOUND, "Method not found"))

    def _write(self, response: dict[str, object]) -> None:
        payload = json.dumps(
            response, ensure_ascii=False, separators=(",", ":")
        ).encode("utf-8")
        self.wfile.write(payload + b"\n")
        self.wfile.flush()


class AiControlServer(socketserver.TCPServer):
    allow_reuse_address = True

    def __init__(self, address: tuple[str, int], state: SampleState) -> None:
        self.state = state
        super().__init__(address, AiControlHandler)


def _game_loop(state: SampleState) -> None:
    while not state.stop.wait(0.01):
        state.process_one()


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run the project-neutral FOnline AiControl protocol sample.",
        allow_abbrev=False,
    )
    parser.add_argument("--host", default=DEFAULT_HOST, help="Listener host.")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="Listener port; zero selects an ephemeral port.")
    parser.add_argument("--token-env", default="FONLINE_AI_TOKEN", help="Environment variable containing the shared token.")
    parser.add_argument("--allow-remote", action="store_true", help="Permit a non-loopback listener; transport remains unencrypted.")
    parser.add_argument("--max-commands", type=int, default=64, help="Bounded command queue capacity.")
    parser.add_argument("--max-events", type=int, default=512, help="Bounded event history capacity.")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    token = os.environ.get(args.token_env, "")
    if not 0 <= args.port <= 65535:
        print("port must be between 0 and 65535", file=sys.stderr)
        return 2
    if args.max_commands <= 0 or args.max_events <= 0:
        print("queue and event capacities must be positive", file=sys.stderr)
        return 2
    if not is_loopback_host(args.host):
        if not args.allow_remote:
            print("non-loopback listeners require --allow-remote", file=sys.stderr)
            return 2
        if not token:
            print("non-loopback listeners require a non-empty token", file=sys.stderr)
            return 2
    state = SampleState(token, args.max_commands, args.max_events)
    try:
        with AiControlServer((args.host, args.port), state) as server:
            state.host = str(server.server_address[0])
            state.port = int(server.server_address[1])
            worker = threading.Thread(target=_game_loop, args=(state,), daemon=True)
            worker.start()
            print(
                "AI_CONTROL_SAMPLE_READY "
                + json.dumps({"host": state.host, "port": state.port}),
                flush=True,
            )
            server.serve_forever(poll_interval=0.05)
    except KeyboardInterrupt:
        pass
    finally:
        state.stop.set()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
