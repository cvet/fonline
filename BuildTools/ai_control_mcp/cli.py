from __future__ import annotations

import argparse
import json
import os
import re
import signal
import sys
from pathlib import Path
from typing import Any

BUILD_TOOLS = Path(__file__).resolve().parents[1]

# Run as a file, the package directory itself would shadow top-level imports with its own modules
if __name__ == "__main__" and Path(sys.path[0] or ".").resolve() == Path(__file__).resolve().parent:
    sys.path.pop(0)

if str(BUILD_TOOLS) not in sys.path:
    sys.path.insert(0, str(BUILD_TOOLS))

from ai_control_client import DEFAULT_HOST, DEFAULT_PORT  # noqa: E402
from ai_control_mcp import ADAPTER_VERSION  # noqa: E402
from ai_control_mcp.context import ToolContext  # noqa: E402
from ai_control_mcp.core_tools import register_core_tools  # noqa: E402
from ai_control_mcp.launch import LaunchProfileError, Orchestrator, load_launch_profile, parse_launch_profile  # noqa: E402
from ai_control_mcp.registry import (  # noqa: E402
    InvalidArguments,
    ProjectModuleError,
    ToolRegistry,
    load_project_module,
    register_project_module,
)
from ai_control_mcp.server import McpServer, ShutdownRequested, log  # noqa: E402
from ai_control_mcp.session import Session  # noqa: E402
from ai_control_mcp.stdio import DEFAULT_MAX_MESSAGE_BYTES, MIN_MESSAGE_BYTES  # noqa: E402


EXIT_OK = 0
EXIT_CONFIGURATION = 2
TOKEN_ENV_PATTERN = "^[A-Za-z_][A-Za-z0-9_]*$"


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ai_control_mcp",
        description="Serve an AiControl bridge to an MCP client over stdio; a development tool, never shipped.",
        allow_abbrev=False,
    )
    parser.add_argument("--host", default=DEFAULT_HOST, help="Default bridge host.")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="Default bridge port and base port for launches.")
    parser.add_argument("--timeout", type=float, default=5.0, help="Bridge socket timeout in seconds.")
    parser.add_argument(
        "--token-env", default="FONLINE_AI_TOKEN", help="Environment variable containing the shared bridge token."
    )
    parser.add_argument(
        "--allow-remote", action="store_true", help="Permit non-loopback bridges; they also require a token."
    )
    parser.add_argument("--project-root", default=".", help="Root for launch paths, binaries and logs.")
    parser.add_argument(
        "--project-module",
        action="append",
        default=[],
        help="Dotted module name or .py file whose register(registry) adds project tools; repeatable.",
    )
    parser.add_argument(
        "--module-path", action="append", default=[], help="Directory added to the import path for dotted project modules."
    )
    parser.add_argument("--launch-profile", help="JSON launch profile; overrides a profile set by a project module.")
    parser.add_argument("--tool-prefix", default="fo_", help="Prefix of the core tool names.")
    parser.add_argument("--server-name", default="fonline-ai-control", help="MCP serverInfo name.")
    parser.add_argument(
        "--keep-processes-on-exit",
        action="store_true",
        help="Leave launched processes running after the adapter exits; their trees must then be stopped manually.",
    )
    parser.add_argument(
        "--max-message-bytes", type=int, default=DEFAULT_MAX_MESSAGE_BYTES, help="Largest accepted MCP message line."
    )
    parser.add_argument(
        "--list-tools", action="store_true", help="Print the registered tool names and descriptions as JSON, then exit."
    )
    return parser


def build_server(args: argparse.Namespace, token: str) -> tuple[McpServer, Orchestrator]:
    project_root = Path(args.project_root).resolve()

    if not project_root.is_dir():
        raise ValueError(f"project root is not a directory: {project_root}")

    session = Session(
        host=args.host, port=args.port, token=token, timeout=args.timeout, allow_remote=args.allow_remote
    )
    registry = ToolRegistry(prefix=args.tool_prefix)
    register_core_tools(registry)
    search_paths = [Path(path) for path in args.module_path]

    for spec in args.project_module:
        register_project_module(registry, load_project_module(spec, search_paths))

    if args.launch_profile:
        profile = load_launch_profile(Path(args.launch_profile))
    elif registry.launch_profile is not None:
        profile = parse_launch_profile(registry.launch_profile)
    else:
        profile = None

    orchestrator = Orchestrator(
        project_root, session, profile, token_env=args.token_env, keep_processes_on_exit=args.keep_processes_on_exit
    )
    context = ToolContext(registry, session, orchestrator, project_root)
    server = McpServer(registry, context, name=args.server_name, version=ADAPTER_VERSION)
    return server, orchestrator


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)

    if args.max_message_bytes < MIN_MESSAGE_BYTES:
        print(f"--max-message-bytes must be at least {MIN_MESSAGE_BYTES}", file=sys.stderr)
        return EXIT_CONFIGURATION

    if not re.fullmatch(TOKEN_ENV_PATTERN, args.token_env):
        print("--token-env must be an environment variable name", file=sys.stderr)
        return EXIT_CONFIGURATION

    token = os.environ.get(args.token_env, "")

    try:
        server, orchestrator = build_server(args, token)
    except (InvalidArguments, LaunchProfileError, ProjectModuleError, ValueError) as error:
        print(f"AiControl MCP adapter configuration failed: {error}", file=sys.stderr)
        return EXIT_CONFIGURATION

    if args.list_tools:
        tools: list[dict[str, Any]] = [{"name": tool.name, "description": tool.description} for tool in server.registry.tools()]
        print(json.dumps(tools, ensure_ascii=False, indent=2))
        return EXIT_OK

    install_shutdown_handlers()

    try:
        server.serve(sys.stdin.buffer, sys.stdout.buffer, args.max_message_bytes)
    except ShutdownRequested:
        # A signal takes the same ownership route as end of input
        pass
    finally:
        stop_owned_processes(orchestrator)

    return EXIT_OK


def install_shutdown_handlers() -> None:
    def request_shutdown(signum: int, frame: Any) -> None:
        raise ShutdownRequested(signum)

    for name in ("SIGTERM", "SIGINT", "SIGBREAK"):
        number = getattr(signal, name, None)

        if number is not None:
            signal.signal(number, request_shutdown)


def stop_owned_processes(orchestrator: Orchestrator) -> None:
    try:
        stopped = orchestrator.stop_all_for_exit()
    except Exception as error:  # noqa: BLE001 - exit cleanup reports a failure but keeps the original exit route
        log(f"adapter exit process cleanup failed: {error}")
        return

    if stopped is not None:
        log("adapter exit stopped owned processes: " + json.dumps(stopped["stopped"], ensure_ascii=False))


if __name__ == "__main__":
    raise SystemExit(main())
