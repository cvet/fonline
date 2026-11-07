"""Tool, resource and prompt registration for the AiControl MCP adapter.

A game project extends the adapter with a Python module that defines ``register(registry)``.
The module receives a ``ToolRegistry`` and adds its own tools, resources, prompts, launch
profile and readiness gate. The core registers only protocol-generic tools.
"""

from __future__ import annotations

import hashlib
import importlib
import importlib.util
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from types import ModuleType
from typing import TYPE_CHECKING, Any, Callable

if TYPE_CHECKING:
    from ai_control_mcp.context import ToolContext


TOOL_NAME_PATTERN = re.compile(r"^[A-Za-z0-9_.-]{1,128}$")
TOOL_PREFIX_PATTERN = re.compile(r"^[A-Za-z0-9_.-]{0,64}$")
MODULE_NAME_PATTERN = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z_][A-Za-z0-9_]*)*$")
RESOURCE_URI_PATTERN = re.compile(r"^[A-Za-z][A-Za-z0-9+.-]*:\S+$")
SCHEMA_TYPES = {"string", "integer", "number", "boolean", "object", "array", "null"}

ToolHandler = Callable[["ToolContext", dict[str, Any]], Any]
ReadinessCheck = Callable[[dict[str, Any], dict[str, Any]], list[str]]


class InvalidArguments(ValueError):
    """Tool arguments the caller must fix; reported as JSON-RPC error -32602."""


class ToolFailure(Exception):
    """A tool ran and failed; reported as a tool result with isError set."""

    def __init__(self, message: str, payload: dict[str, Any] | None = None) -> None:
        super().__init__(message)
        self.payload = payload if payload is not None else {"error": message}


class ProjectModuleError(RuntimeError):
    pass


@dataclass(frozen=True)
class ToolResult:
    content: list[dict[str, Any]]
    is_error: bool = False

    @staticmethod
    def json(payload: Any, is_error: bool = False) -> ToolResult:
        text = json.dumps(payload, ensure_ascii=False, indent=2, sort_keys=False)
        return ToolResult([{"type": "text", "text": text}], is_error)

    @staticmethod
    def text(text: str, is_error: bool = False) -> ToolResult:
        return ToolResult([{"type": "text", "text": text}], is_error)

    def to_mcp(self) -> dict[str, Any]:
        return {"content": list(self.content), "isError": self.is_error}


@dataclass(frozen=True)
class Tool:
    name: str
    description: str
    input_schema: dict[str, Any]
    handler: ToolHandler
    annotations: dict[str, Any] | None = None

    def to_mcp(self) -> dict[str, Any]:
        result: dict[str, Any] = {"name": self.name, "description": self.description, "inputSchema": self.input_schema}

        if self.annotations:
            result["annotations"] = dict(self.annotations)

        return result


@dataclass(frozen=True)
class Resource:
    uri: str
    name: str
    description: str
    mime_type: str
    reader: Callable[["ToolContext"], str]

    def to_mcp(self) -> dict[str, Any]:
        return {"uri": self.uri, "name": self.name, "description": self.description, "mimeType": self.mime_type}


@dataclass(frozen=True)
class Prompt:
    name: str
    description: str
    arguments: list[dict[str, Any]]
    render: Callable[[dict[str, str]], str]

    def to_mcp(self) -> dict[str, Any]:
        return {"name": self.name, "description": self.description, "arguments": list(self.arguments)}


@dataclass
class ToolRegistry:
    prefix: str = "fo_"
    instructions: str = ""
    launch_profile: dict[str, Any] | None = None
    readiness_check: ReadinessCheck | None = None
    readiness_fields: tuple[str, ...] = ()
    _tools: dict[str, Tool] = field(default_factory=dict)
    _resources: dict[str, Resource] = field(default_factory=dict)
    _prompts: dict[str, Prompt] = field(default_factory=dict)

    def __post_init__(self) -> None:
        if not TOOL_PREFIX_PATTERN.fullmatch(self.prefix):
            raise ValueError(f"tool prefix must match {TOOL_PREFIX_PATTERN.pattern}: {self.prefix!r}")

    def core_name(self, base_name: str) -> str:
        return self.prefix + base_name

    def add_tool(
        self,
        name: str,
        handler: ToolHandler,
        *,
        description: str,
        input_schema: dict[str, Any] | None = None,
        annotations: dict[str, Any] | None = None,
        replace: bool = False,
    ) -> Tool:
        if not TOOL_NAME_PATTERN.fullmatch(name):
            raise ValueError(f"tool name must match {TOOL_NAME_PATTERN.pattern}: {name!r}")
        if not callable(handler):
            raise ValueError(f"tool {name} handler must be callable")
        if not isinstance(description, str) or not description.strip():
            raise ValueError(f"tool {name} needs a non-empty description")
        if replace and name not in self._tools:
            raise ValueError(f"tool {name} cannot replace a tool that is not registered")
        if not replace and name in self._tools:
            raise ValueError(f"tool {name} is already registered; pass replace=True to override it explicitly")

        schema = object_schema({}) if input_schema is None else input_schema
        check_schema(schema, f"tool {name} inputSchema")
        tool = Tool(name, description.strip(), schema, handler, annotations)
        self._tools[name] = tool
        return tool

    def tool(
        self,
        name: str,
        *,
        description: str,
        input_schema: dict[str, Any] | None = None,
        annotations: dict[str, Any] | None = None,
        replace: bool = False,
    ) -> Callable[[ToolHandler], ToolHandler]:
        def decorator(handler: ToolHandler) -> ToolHandler:
            self.add_tool(
                name, handler, description=description, input_schema=input_schema, annotations=annotations, replace=replace
            )
            return handler

        return decorator

    def add_resource(
        self, uri: str, reader: Callable[["ToolContext"], str], *, name: str, description: str, mime_type: str = "text/markdown"
    ) -> None:
        if not RESOURCE_URI_PATTERN.fullmatch(uri):
            raise ValueError(f"resource uri must be an absolute URI: {uri!r}")
        if uri in self._resources:
            raise ValueError(f"resource {uri} is already registered")
        if not callable(reader):
            raise ValueError(f"resource {uri} reader must be callable")

        self._resources[uri] = Resource(uri, name, description, mime_type, reader)

    def add_prompt(
        self,
        name: str,
        render: Callable[[dict[str, str]], str],
        *,
        description: str,
        arguments: list[dict[str, Any]] | None = None,
    ) -> None:
        if not TOOL_NAME_PATTERN.fullmatch(name):
            raise ValueError(f"prompt name must match {TOOL_NAME_PATTERN.pattern}: {name!r}")
        if name in self._prompts:
            raise ValueError(f"prompt {name} is already registered")
        if not callable(render):
            raise ValueError(f"prompt {name} renderer must be callable")

        self._prompts[name] = Prompt(name, description, list(arguments or []), render)

    def set_launch_profile(self, profile: dict[str, Any]) -> None:
        if not isinstance(profile, dict):
            raise ValueError("launch profile must be a JSON object")

        self.launch_profile = profile

    def set_readiness(self, check: ReadinessCheck | None = None, *, fields: list[str] | tuple[str, ...] = ()) -> None:
        """Install the project readiness gate: a callable returning missing gate names, or truthy observation fields."""
        if check is not None and not callable(check):
            raise ValueError("readiness check must be callable")
        if any(not isinstance(name, str) or not name for name in fields):
            raise ValueError("readiness fields must be non-empty strings")

        self.readiness_check = check
        self.readiness_fields = tuple(fields)

    def set_instructions(self, text: str) -> None:
        self.instructions = str(text)

    def tools(self) -> list[Tool]:
        return list(self._tools.values())

    def get_tool(self, name: str) -> Tool | None:
        return self._tools.get(name)

    def resources(self) -> list[Resource]:
        return list(self._resources.values())

    def get_resource(self, uri: str) -> Resource | None:
        return self._resources.get(uri)

    def prompts(self) -> list[Prompt]:
        return list(self._prompts.values())

    def get_prompt(self, name: str) -> Prompt | None:
        return self._prompts.get(name)


def object_schema(properties: dict[str, Any], required: list[str] | None = None, *, additional: bool = False) -> dict[str, Any]:
    schema: dict[str, Any] = {"type": "object", "properties": dict(properties), "additionalProperties": additional}

    if required:
        schema["required"] = list(required)

    return schema


def check_schema(schema: Any, label: str) -> None:
    """Reject schemas that use keywords the adapter's validator does not enforce."""
    if not isinstance(schema, dict):
        raise ValueError(f"{label} must be an object")

    supported = {
        "type", "description", "properties", "required", "additionalProperties", "items", "enum",
        "minimum", "maximum", "minLength", "maxLength", "minItems", "maxItems", "default", "title",
    }
    unknown = sorted(set(schema) - supported)

    if unknown:
        raise ValueError(f"{label} uses unsupported schema keywords: {', '.join(unknown)}")

    types = schema.get("type")

    if types is not None:
        for name in types if isinstance(types, list) else [types]:
            if name not in SCHEMA_TYPES:
                raise ValueError(f"{label} has unsupported type {name!r}")

    for name, child in (schema.get("properties") or {}).items():
        check_schema(child, f"{label}.properties.{name}")

    if isinstance(schema.get("items"), dict):
        check_schema(schema["items"], f"{label}.items")

    additional = schema.get("additionalProperties")

    if isinstance(additional, dict):
        check_schema(additional, f"{label}.additionalProperties")


def validate_arguments(schema: dict[str, Any], value: Any, path: str = "arguments") -> None:
    """Validate a value against the supported JSON Schema subset; raise InvalidArguments on the first mismatch."""
    types = schema.get("type")

    if types is not None:
        allowed = types if isinstance(types, list) else [types]

        if not any(_matches_type(value, name) for name in allowed):
            raise InvalidArguments(f"{path} must be {' or '.join(allowed)}")

    if "enum" in schema and value not in schema["enum"]:
        raise InvalidArguments(f"{path} must be one of {json.dumps(schema['enum'], ensure_ascii=False)}")

    if _matches_type(value, "number"):
        if "minimum" in schema and value < schema["minimum"]:
            raise InvalidArguments(f"{path} must be >= {schema['minimum']}")
        if "maximum" in schema and value > schema["maximum"]:
            raise InvalidArguments(f"{path} must be <= {schema['maximum']}")

    if isinstance(value, str):
        if "minLength" in schema and len(value) < schema["minLength"]:
            raise InvalidArguments(f"{path} must have at least {schema['minLength']} characters")
        if "maxLength" in schema and len(value) > schema["maxLength"]:
            raise InvalidArguments(f"{path} must have at most {schema['maxLength']} characters")

    if isinstance(value, list):
        if "minItems" in schema and len(value) < schema["minItems"]:
            raise InvalidArguments(f"{path} must have at least {schema['minItems']} items")
        if "maxItems" in schema and len(value) > schema["maxItems"]:
            raise InvalidArguments(f"{path} must have at most {schema['maxItems']} items")

        items = schema.get("items")

        if isinstance(items, dict):
            for index, item in enumerate(value):
                validate_arguments(items, item, f"{path}[{index}]")

    if isinstance(value, dict):
        properties = schema.get("properties") or {}

        for name in schema.get("required") or []:
            if name not in value:
                raise InvalidArguments(f"{path}.{name} is required")

        additional = schema.get("additionalProperties", True)

        for name, item in value.items():
            if name in properties:
                validate_arguments(properties[name], item, f"{path}.{name}")
            elif additional is False:
                raise InvalidArguments(f"{path}.{name} is not a known argument")
            elif isinstance(additional, dict):
                validate_arguments(additional, item, f"{path}.{name}")


def _matches_type(value: Any, name: str) -> bool:
    if name == "string":
        return isinstance(value, str)
    if name == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if name == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if name == "boolean":
        return isinstance(value, bool)
    if name == "object":
        return isinstance(value, dict)
    if name == "array":
        return isinstance(value, list)
    return value is None


def load_project_module(spec: str, search_paths: list[Path]) -> ModuleType:
    """Import a project module by dotted name or by ``.py`` file path."""
    if spec.endswith(".py") or "/" in spec or "\\" in spec:
        path = Path(spec).resolve()

        if not path.is_file():
            raise ProjectModuleError(f"project module file not found: {path}")

        module_name = "ai_control_mcp_project_" + hashlib.sha256(str(path).encode("utf-8")).hexdigest()[:16]
        module_spec = importlib.util.spec_from_file_location(module_name, path)

        if module_spec is None or module_spec.loader is None:
            raise ProjectModuleError(f"project module cannot be loaded: {path}")

        module = importlib.util.module_from_spec(module_spec)
        # A project module may import its sibling helper modules
        sys.path.insert(0, str(path.parent))
        sys.modules[module_name] = module
        module_spec.loader.exec_module(module)
        return module

    if not MODULE_NAME_PATTERN.fullmatch(spec):
        raise ProjectModuleError(f"project module must be a dotted module name or a .py file path: {spec!r}")

    for search_path in reversed(search_paths):
        sys.path.insert(0, str(search_path.resolve()))

    try:
        return importlib.import_module(spec)
    except ModuleNotFoundError as exception:
        raise ProjectModuleError(f"project module {spec!r} was not found: {exception}") from exception


def register_project_module(registry: ToolRegistry, module: ModuleType) -> None:
    register = getattr(module, "register", None)

    if not callable(register):
        raise ProjectModuleError(f"project module {module.__name__} must define register(registry)")

    register(registry)
