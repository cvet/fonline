"""MCP request dispatch and the stdio serve loop."""

from __future__ import annotations

import sys
from typing import Any, BinaryIO

from ai_control_client import AiControlError, RemoteError
from ai_control_mcp.context import ToolContext
from ai_control_mcp.registry import InvalidArguments, ToolFailure, ToolResult, ToolRegistry, validate_arguments
from ai_control_mcp.stdio import (
    INTERNAL_ERROR,
    INVALID_PARAMS,
    INVALID_REQUEST,
    JSONRPC_VERSION,
    METHOD_NOT_FOUND,
    FrameError,
    decode_frame,
    error_message,
    read_frame,
    result_message,
    write_frame,
)


SUPPORTED_PROTOCOL_VERSIONS = ("2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05")


class ShutdownRequested(BaseException):
    """Raised from a signal handler; a BaseException so per-request error handling cannot swallow it."""


class McpServer:
    def __init__(self, registry: ToolRegistry, context: ToolContext, *, name: str, version: str) -> None:
        self.registry = registry
        self.context = context
        self.name = name
        self.version = version

    def handle(self, message: Any) -> dict[str, Any] | None:
        if isinstance(message, list):
            return error_message(None, INVALID_REQUEST, "batch requests are not supported")

        if not isinstance(message, dict):
            return error_message(None, INVALID_REQUEST, "message must be a JSON object")

        if "method" not in message:
            # A response to a server-initiated request; this server sends none
            return None

        is_notification = "id" not in message
        request_id = message.get("id")
        method = message.get("method")

        if message.get("jsonrpc") != JSONRPC_VERSION or not isinstance(method, str) or not method:
            return None if is_notification else error_message(_safe_id(request_id), INVALID_REQUEST, "invalid JSON-RPC request")

        if is_notification:
            return None

        if isinstance(request_id, bool) or not isinstance(request_id, (str, int)):
            return error_message(None, INVALID_REQUEST, "request id must be a string or an integer")

        params = message.get("params", {})

        if not isinstance(params, dict):
            return error_message(request_id, INVALID_PARAMS, "params must be an object")

        handler = {
            "initialize": self._initialize,
            "ping": lambda _: {},
            "tools/list": lambda _: {"tools": [tool.to_mcp() for tool in self.registry.tools()]},
            "tools/call": self._call_tool,
            "resources/list": lambda _: {"resources": [resource.to_mcp() for resource in self.registry.resources()]},
            "resources/read": self._read_resource,
            "prompts/list": lambda _: {"prompts": [prompt.to_mcp() for prompt in self.registry.prompts()]},
            "prompts/get": self._get_prompt,
        }.get(method)

        if handler is None:
            return error_message(request_id, METHOD_NOT_FOUND, f"unknown method: {method}")

        try:
            return result_message(request_id, handler(params))
        except InvalidArguments as exception:
            return error_message(request_id, INVALID_PARAMS, str(exception))
        except Exception as exception:  # noqa: BLE001 - one failing request must not end the stdio session
            return error_message(request_id, INTERNAL_ERROR, f"{type(exception).__name__}: {exception}")

    def _initialize(self, params: dict[str, Any]) -> dict[str, Any]:
        requested = params.get("protocolVersion")
        version = requested if requested in SUPPORTED_PROTOCOL_VERSIONS else SUPPORTED_PROTOCOL_VERSIONS[0]
        capabilities: dict[str, Any] = {"tools": {"listChanged": False}}

        if self.registry.resources():
            capabilities["resources"] = {"subscribe": False, "listChanged": False}

        if self.registry.prompts():
            capabilities["prompts"] = {"listChanged": False}

        result: dict[str, Any] = {
            "protocolVersion": version,
            "capabilities": capabilities,
            "serverInfo": {"name": self.name, "version": self.version},
        }

        if self.registry.instructions:
            result["instructions"] = self.registry.instructions

        return result

    def _call_tool(self, params: dict[str, Any]) -> dict[str, Any]:
        name = params.get("name")
        arguments = params.get("arguments", {})

        if not isinstance(name, str):
            raise InvalidArguments("tools/call needs a tool name")

        if arguments is None:
            arguments = {}

        if not isinstance(arguments, dict):
            raise InvalidArguments("tool arguments must be an object")

        tool = self.registry.get_tool(name)

        if tool is None:
            raise InvalidArguments(f"unknown tool: {name}")

        validate_arguments(tool.input_schema, arguments)

        try:
            outcome = tool.handler(self.context, arguments)
        except ToolFailure as failure:
            return ToolResult.json(failure.payload, is_error=True).to_mcp()
        except RemoteError as error:
            return ToolResult.json({"error": {"code": error.code, "message": error.message, "data": error.data}}, is_error=True).to_mcp()
        except (AiControlError, OSError) as error:
            return ToolResult.json({"error": {"type": type(error).__name__, "message": str(error)}}, is_error=True).to_mcp()

        if isinstance(outcome, ToolResult):
            return outcome.to_mcp()

        return ToolResult.json(outcome).to_mcp()

    def _read_resource(self, params: dict[str, Any]) -> dict[str, Any]:
        uri = params.get("uri")
        resource = self.registry.get_resource(uri) if isinstance(uri, str) else None

        if resource is None:
            raise InvalidArguments(f"unknown resource uri: {uri}")

        text = resource.reader(self.context)
        return {"contents": [{"uri": resource.uri, "mimeType": resource.mime_type, "text": text}]}

    def _get_prompt(self, params: dict[str, Any]) -> dict[str, Any]:
        name = params.get("name")
        prompt = self.registry.get_prompt(name) if isinstance(name, str) else None

        if prompt is None:
            raise InvalidArguments(f"unknown prompt: {name}")

        arguments = params.get("arguments") or {}

        if not isinstance(arguments, dict) or any(not isinstance(value, str) for value in arguments.values()):
            raise InvalidArguments("prompt arguments must be an object of strings")

        for argument in prompt.arguments:
            if argument.get("required") and argument.get("name") not in arguments:
                raise InvalidArguments(f"prompt argument {argument.get('name')} is required")

        text = prompt.render(arguments)
        return {"description": prompt.description, "messages": [{"role": "user", "content": {"type": "text", "text": text}}]}

    def serve(self, input_stream: BinaryIO, output_stream: BinaryIO, max_message_bytes: int) -> None:
        """Serve until end of input; the caller owns process cleanup after this returns or raises."""
        while True:
            try:
                frame = read_frame(input_stream, max_message_bytes)

                if frame is None:
                    return

                if not frame.strip():
                    continue

                response = self.handle(decode_frame(frame))
            except FrameError as error:
                response = error_message(None, error.code, error.message)

            if response is not None:
                write_frame(output_stream, response)


def _safe_id(value: Any) -> Any:
    if isinstance(value, bool) or not isinstance(value, (str, int)):
        return None

    return value


def log(message: str) -> None:
    sys.stderr.write(message + "\n")
    sys.stderr.flush()
