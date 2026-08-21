from __future__ import annotations

import argparse
import ipaddress
import json
import os
import socket
import sys
from pathlib import Path
from typing import Any


JSONRPC_VERSION = "2.0"
PROTOCOL_VERSION = 1
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 43011
MAX_LINE_BYTES = 1024 * 1024
METHODS = ("auth", "ping", "status", "observe", "events", "act")


class AiControlError(RuntimeError):
    pass


class ProtocolError(AiControlError):
    pass


class RemoteError(AiControlError):
    def __init__(self, code: int, message: str, data: object = None) -> None:
        super().__init__(f"remote error {code}: {message}")
        self.code = code
        self.message = message
        self.data = data


def is_loopback_host(host: str) -> bool:
    if host.lower() == "localhost":
        return True
    try:
        return ipaddress.ip_address(host).is_loopback
    except ValueError:
        return False


class AiControlClient:
    def __init__(
        self,
        host: str = DEFAULT_HOST,
        port: int = DEFAULT_PORT,
        *,
        timeout: float = 5.0,
        allow_remote: bool = False,
        max_line_bytes: int = MAX_LINE_BYTES,
    ) -> None:
        if not 1 <= port <= 65535:
            raise ValueError("port must be between 1 and 65535")
        if timeout <= 0:
            raise ValueError("timeout must be positive")
        if max_line_bytes < 256:
            raise ValueError("max_line_bytes must be at least 256")
        if not allow_remote and not is_loopback_host(host):
            raise ValueError(
                "non-loopback AiControl endpoints require explicit allow_remote=True"
            )
        self.host = host
        self.port = port
        self.timeout = timeout
        self.max_line_bytes = max_line_bytes
        self._socket: socket.socket | None = None
        self._reader: Any = None
        self._writer: Any = None
        self._next_id = 1

    def connect(self) -> None:
        if self._socket is not None:
            return
        connection = socket.create_connection(
            (self.host, self.port), timeout=self.timeout
        )
        connection.settimeout(self.timeout)
        self._socket = connection
        self._reader = connection.makefile("rb")
        self._writer = connection.makefile("wb")

    def close(self) -> None:
        for stream_name in ("_reader", "_writer"):
            stream = getattr(self, stream_name)
            if stream is not None:
                try:
                    stream.close()
                finally:
                    setattr(self, stream_name, None)
        if self._socket is not None:
            try:
                self._socket.close()
            finally:
                self._socket = None

    def __enter__(self) -> AiControlClient:
        self.connect()
        return self

    def __exit__(self, *_: object) -> None:
        self.close()

    def request(self, method: str, params: dict[str, object] | None = None) -> Any:
        if not isinstance(method, str) or not method:
            raise ValueError("method must be a non-empty string")
        if params is not None and not isinstance(params, dict):
            raise ValueError("params must be an object")
        self.connect()
        request_id = self._next_id
        self._next_id += 1
        request = {
            "jsonrpc": JSONRPC_VERSION,
            "id": request_id,
            "method": method,
            "params": params or {},
        }
        payload = json.dumps(
            request, ensure_ascii=False, separators=(",", ":")
        ).encode("utf-8")
        if len(payload) > self.max_line_bytes:
            raise ProtocolError("request exceeds the configured line limit")
        assert self._writer is not None
        self._writer.write(payload + b"\n")
        self._writer.flush()

        assert self._reader is not None
        response_line = self._reader.readline(self.max_line_bytes + 2)
        if not response_line:
            raise ProtocolError("bridge closed the connection without a response")
        if not response_line.endswith(b"\n"):
            if len(response_line) > self.max_line_bytes:
                raise ProtocolError("response exceeds the configured line limit")
            raise ProtocolError("response is not newline terminated")
        if len(response_line) - 1 > self.max_line_bytes:
            raise ProtocolError("response exceeds the configured line limit")
        try:
            response = json.loads(response_line[:-1].decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exception:
            raise ProtocolError(f"response is not valid UTF-8 JSON: {exception}") from exception
        if not isinstance(response, dict):
            raise ProtocolError("response must be a JSON object")
        if response.get("jsonrpc") != JSONRPC_VERSION:
            raise ProtocolError("response has an unsupported jsonrpc value")
        if response.get("id") != request_id:
            raise ProtocolError("response id does not match the request")
        has_result = "result" in response
        has_error = "error" in response
        if has_result == has_error:
            raise ProtocolError("response must contain exactly one of result or error")
        if has_error:
            error = response["error"]
            if (
                not isinstance(error, dict)
                or not isinstance(error.get("code"), int)
                or not isinstance(error.get("message"), str)
            ):
                raise ProtocolError("response error has an invalid shape")
            raise RemoteError(error["code"], error["message"], error.get("data"))
        return response["result"]

    def auth(self, token: str) -> bool:
        result = self.request("auth", {"token": token})
        if not isinstance(result, dict) or not isinstance(
            result.get("authorized"), bool
        ):
            raise ProtocolError("auth result has an invalid shape")
        return result["authorized"]

    def ping(self) -> dict[str, object]:
        return self._object_result("ping", {})

    def status(self) -> dict[str, object]:
        return self._object_result("status", {})

    def observe(self) -> dict[str, object]:
        return self._object_result("observe", {})

    def events(self, *, after_seq: int = 0, limit: int = 100) -> dict[str, object]:
        if after_seq < 0:
            raise ValueError("after_seq must not be negative")
        if limit <= 0:
            raise ValueError("limit must be positive")
        return self._object_result(
            "events", {"afterSeq": after_seq, "limit": limit}
        )

    def act(self, command: dict[str, object]) -> dict[str, object]:
        if not isinstance(command, dict):
            raise ValueError("command must be an object")
        command_type = command.get("type")
        if not isinstance(command_type, str) or not command_type:
            raise ValueError("command.type must be a non-empty string")
        return self._object_result("act", command)

    def _object_result(
        self, method: str, params: dict[str, object]
    ) -> dict[str, object]:
        result = self.request(method, params)
        if not isinstance(result, dict):
            raise ProtocolError(f"{method} result must be an object")
        return result


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ai_control_client.py",
        description="Call an Engine-compatible AiControl bridge over NDJSON/TCP.",
        allow_abbrev=False,
    )
    parser.add_argument("--host", default=DEFAULT_HOST, help="Bridge host.")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="Bridge port.")
    parser.add_argument(
        "--timeout", type=float, default=5.0, help="Socket timeout in seconds."
    )
    parser.add_argument(
        "--token-env",
        default="FONLINE_AI_TOKEN",
        help="Environment variable containing the shared token.",
    )
    parser.add_argument(
        "--allow-remote",
        action="store_true",
        help="Permit a non-loopback endpoint; transport remains unencrypted.",
    )
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("ping", help="Check bridge liveness.")
    commands.add_parser("status", help="Read bridge status and queue limits.")
    commands.add_parser("observe", help="Read the latest project observation.")
    events = commands.add_parser("events", help="Read events after a sequence cursor.")
    events.add_argument("--after-seq", type=int, default=0, help="Exclusive event cursor.")
    events.add_argument("--limit", type=int, default=100, help="Maximum events to return.")
    act = commands.add_parser("act", help="Enqueue one project-defined command.")
    act.add_argument("--type", required=True, help="Project-defined command type.")
    act.add_argument("--target-id", help="Optional target entity identifier.")
    act.add_argument("--item-id", help="Optional item entity identifier.")
    act.add_argument("--aux-id", help="Optional auxiliary entity identifier.")
    act.add_argument("--x", type=int, help="Optional project world X coordinate.")
    act.add_argument("--y", type=int, help="Optional project world Y coordinate.")
    act.add_argument("--screen-x", type=int, help="Optional screen X coordinate.")
    act.add_argument("--screen-y", type=int, help="Optional screen Y coordinate.")
    act.add_argument("--int-arg", type=int, help="Optional integer payload.")
    act.add_argument("--string-arg", help="Optional string payload.")
    act.add_argument("--append", action="store_true", help="Request project queue append semantics.")
    return parser


def _act_params(args: argparse.Namespace) -> dict[str, object]:
    result: dict[str, object] = {"type": args.type}
    for argument, field in (
        ("target_id", "targetId"),
        ("item_id", "itemId"),
        ("aux_id", "auxId"),
        ("x", "x"),
        ("y", "y"),
        ("screen_x", "screenX"),
        ("screen_y", "screenY"),
        ("int_arg", "intArg"),
        ("string_arg", "stringArg"),
    ):
        value = getattr(args, argument)
        if value is not None:
            result[field] = value
    if args.append:
        result["append"] = True
    return result


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    token = os.environ.get(args.token_env, "")
    try:
        with AiControlClient(
            args.host,
            args.port,
            timeout=args.timeout,
            allow_remote=args.allow_remote,
        ) as client:
            if token and not client.auth(token):
                raise AiControlError("bridge rejected the configured token")
            if args.command == "ping":
                result = client.ping()
            elif args.command == "status":
                result = client.status()
            elif args.command == "observe":
                result = client.observe()
            elif args.command == "events":
                result = client.events(after_seq=args.after_seq, limit=args.limit)
            elif args.command == "act":
                result = client.act(_act_params(args))
            else:
                raise AssertionError(f"unsupported command: {args.command}")
    except (AiControlError, OSError, ValueError) as exception:
        print(f"AiControl request failed: {exception}", file=sys.stderr)
        return 1
    print(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
