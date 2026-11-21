"""MCP stdio framing: one UTF-8 JSON-RPC 2.0 message per newline-terminated line."""

from __future__ import annotations

import json
from typing import Any, BinaryIO


JSONRPC_VERSION = "2.0"
DEFAULT_MAX_MESSAGE_BYTES = 4 * 1024 * 1024
MIN_MESSAGE_BYTES = 1024

PARSE_ERROR = -32700
INVALID_REQUEST = -32600
METHOD_NOT_FOUND = -32601
INVALID_PARAMS = -32602
INTERNAL_ERROR = -32603


class FrameError(Exception):
    def __init__(self, code: int, message: str) -> None:
        super().__init__(message)
        self.code = code
        self.message = message


def read_frame(stream: BinaryIO, max_bytes: int) -> bytes | None:
    """Return one message line without its terminator, or None at end of input.

    An oversized line is consumed up to its terminator before FrameError is raised, so the next
    read starts at the following message instead of in the middle of the rejected one.
    """
    line = stream.readline(max_bytes + 2)

    if not line:
        return None

    if line.endswith(b"\n"):
        body = line[:-1]

        if body.endswith(b"\r"):
            body = body[:-1]

        if len(body) > max_bytes:
            raise FrameError(INVALID_REQUEST, f"message exceeds {max_bytes} bytes")

        return body

    if len(line) <= max_bytes:
        raise FrameError(PARSE_ERROR, "message is not newline terminated")

    while True:
        rest = stream.readline(65536)

        if not rest or rest.endswith(b"\n"):
            break

    raise FrameError(INVALID_REQUEST, f"message exceeds {max_bytes} bytes")


def decode_frame(frame: bytes) -> Any:
    try:
        return json.loads(frame.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exception:
        raise FrameError(PARSE_ERROR, f"message is not valid UTF-8 JSON: {exception}") from exception


def write_frame(stream: BinaryIO, message: dict[str, Any]) -> None:
    payload = json.dumps(message, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    stream.write(payload + b"\n")
    stream.flush()


def result_message(request_id: Any, result: dict[str, Any]) -> dict[str, Any]:
    return {"jsonrpc": JSONRPC_VERSION, "id": request_id, "result": result}


def error_message(request_id: Any, code: int, message: str, data: Any = None) -> dict[str, Any]:
    error: dict[str, Any] = {"code": code, "message": message}

    if data is not None:
        error["data"] = data

    return {"jsonrpc": JSONRPC_VERSION, "id": request_id, "error": error}
