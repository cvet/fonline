from __future__ import annotations

import io
import json
import os
import queue
import socket
import socketserver
import subprocess
import sys
import textwrap
import threading
import time
from pathlib import Path
from typing import Any, Callable, Iterator
from unittest.mock import patch

import pytest


ENGINE_ROOT = Path(__file__).resolve().parents[2]
BUILD_TOOLS = ENGINE_ROOT / "BuildTools"
SAMPLE_DIR = ENGINE_ROOT / "Examples" / "AiControlSample"
ADAPTER = BUILD_TOOLS / "ai_control_mcp"
sys.path.insert(0, str(BUILD_TOOLS))
sys.path.insert(0, str(SAMPLE_DIR))

import ai_control_sample  # noqa: E402
from ai_control_mcp import advisory, cli, process_owner, runner  # noqa: E402
from ai_control_mcp.context import ToolContext  # noqa: E402
from ai_control_mcp.core_tools import register_core_tools  # noqa: E402
from ai_control_mcp.launch import (  # noqa: E402
    LaunchProfileError,
    Orchestrator,
    format_setting_value,
    parse_launch_profile,
)
from ai_control_mcp.registry import (  # noqa: E402
    InvalidArguments,
    ProjectModuleError,
    ToolRegistry,
    load_project_module,
    object_schema,
    register_project_module,
    validate_arguments,
)
from ai_control_mcp.server import McpServer  # noqa: E402
from ai_control_mcp.session import Session  # noqa: E402
from ai_control_mcp.stdio import read_frame  # noqa: E402

OWNERSHIP_PLATFORM = sys.platform == "win32" or sys.platform.startswith("linux")
CORE_TOOLS = [
    "status", "ping", "observe", "events", "next_events", "act", "wait_command", "connect", "endpoints",
    "wait_ready", "wait_all_ready", "launch_options", "launch", "processes", "stop_process", "logs",
]


# Fixtures and helpers


class SampleBridge:
    def __init__(self, token: str = "", run_loop: bool = True, max_events: int = 64) -> None:
        self.state = ai_control_sample.SampleState(token, 16, max_events)
        self.server = ai_control_sample.AiControlServer(("127.0.0.1", 0), self.state)
        self.port = int(self.server.server_address[1])
        self._threads = [threading.Thread(target=self.server.serve_forever, kwargs={"poll_interval": 0.05}, daemon=True)]

        if run_loop:
            self._threads.append(threading.Thread(target=ai_control_sample._game_loop, args=(self.state,), daemon=True))

        for thread in self._threads:
            thread.start()

    def close(self) -> None:
        self.state.stop.set()
        self.server.shutdown()
        self.server.server_close()

        for thread in self._threads:
            thread.join(timeout=5)


class ScriptedPeer:
    """A malformed or scripted AiControl peer: answers each request line with responder(request)."""

    def __init__(self, responder: Callable[[dict[str, Any]], bytes | None]) -> None:
        self.responder = responder
        self.requests: list[dict[str, Any]] = []
        peer = self

        class Handler(socketserver.StreamRequestHandler):
            def handle(self) -> None:
                while True:
                    line = self.rfile.readline()

                    if not line:
                        return

                    request = json.loads(line)
                    peer.requests.append(request)
                    reply = peer.responder(request)

                    if reply is None:
                        return

                    self.wfile.write(reply)
                    self.wfile.flush()

        class Server(socketserver.ThreadingTCPServer):
            daemon_threads = True
            allow_reuse_address = True

        self.server = Server(("127.0.0.1", 0), Handler)
        self.port = int(self.server.server_address[1])
        self.thread = threading.Thread(target=self.server.serve_forever, kwargs={"poll_interval": 0.05}, daemon=True)
        self.thread.start()

    def close(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)


def reply(request: dict[str, Any], result: Any) -> bytes:
    return json.dumps({"jsonrpc": "2.0", "id": request["id"], "result": result}).encode("utf-8") + b"\n"


@pytest.fixture
def bridge() -> Iterator[SampleBridge]:
    sample = SampleBridge()
    yield sample
    sample.close()


def make_server(
    tmp_path: Path,
    port: int,
    *,
    token: str = "",
    allow_remote: bool = False,
    profile: dict[str, Any] | None = None,
    registry: ToolRegistry | None = None,
    timeout: float = 2.0,
) -> McpServer:
    root = tmp_path.resolve()
    session = Session(host="127.0.0.1", port=port, token=token, timeout=timeout, allow_remote=allow_remote)

    if registry is None:
        registry = ToolRegistry(prefix="fo_")
        register_core_tools(registry)

    orchestrator = Orchestrator(
        root, session, parse_launch_profile(profile) if profile else None, token_env="FONLINE_AI_TOKEN"
    )
    return McpServer(registry, ToolContext(registry, session, orchestrator, root), name="test", version="1")


def rpc(server: McpServer, method: str, params: dict[str, Any] | None = None, request_id: Any = 1) -> dict[str, Any]:
    response = server.handle({"jsonrpc": "2.0", "id": request_id, "method": method, "params": params or {}})
    assert response is not None
    return response


def tool(server: McpServer, name: str, arguments: dict[str, Any] | None = None) -> tuple[dict[str, Any], bool]:
    response = rpc(server, "tools/call", {"name": name, "arguments": arguments or {}})
    assert "result" in response, response
    return json.loads(response["result"]["content"][0]["text"]), bool(response["result"]["isError"])


def tool_ok(server: McpServer, name: str, arguments: dict[str, Any] | None = None) -> dict[str, Any]:
    payload, is_error = tool(server, name, arguments)
    assert not is_error, payload
    return payload


def free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


# MCP framing and dispatch


def test_initialize_negotiates_the_protocol_version_and_capabilities(tmp_path: Path) -> None:
    server = make_server(tmp_path, 43011)

    accepted = rpc(server, "initialize", {"protocolVersion": "2025-06-18"})["result"]
    fallback = rpc(server, "initialize", {"protocolVersion": "1999-01-01"})["result"]

    assert accepted["protocolVersion"] == "2025-06-18"
    assert fallback["protocolVersion"] == "2025-11-25"
    assert accepted["capabilities"] == {"tools": {"listChanged": False}}
    assert accepted["serverInfo"] == {"name": "test", "version": "1"}
    assert "instructions" not in accepted

    server.registry.set_instructions("Read the project guide first.")
    server.registry.add_resource("game://guide", lambda context: "guide", name="guide", description="Guide")
    server.registry.add_prompt("game_guide", lambda arguments: "go", description="Guide prompt")
    extended = rpc(server, "initialize", {"protocolVersion": "2025-11-25"})["result"]

    assert extended["instructions"] == "Read the project guide first."
    assert set(extended["capabilities"]) == {"tools", "resources", "prompts"}


@pytest.mark.parametrize(
    ("message", "code"),
    [
        ([{"jsonrpc": "2.0", "id": 1, "method": "ping"}], -32600),
        ("ping", -32600),
        ({"jsonrpc": "1.0", "id": 1, "method": "ping"}, -32600),
        ({"jsonrpc": "2.0", "id": 1, "method": ""}, -32600),
        ({"jsonrpc": "2.0", "id": True, "method": "ping"}, -32600),
        ({"jsonrpc": "2.0", "id": None, "method": "ping"}, -32600),
        ({"jsonrpc": "2.0", "id": 1, "method": "ping", "params": []}, -32602),
        ({"jsonrpc": "2.0", "id": 1, "method": "sampling/createMessage"}, -32601),
    ],
)
def test_malformed_requests_get_json_rpc_errors(tmp_path: Path, message: Any, code: int) -> None:
    response = make_server(tmp_path, 43011).handle(message)

    assert response is not None
    assert response["error"]["code"] == code


def test_notifications_and_client_responses_get_no_reply(tmp_path: Path) -> None:
    server = make_server(tmp_path, 43011)

    assert server.handle({"jsonrpc": "2.0", "method": "notifications/initialized"}) is None
    assert server.handle({"jsonrpc": "2.0", "method": "tools/call", "params": {"name": "fo_launch"}}) is None
    assert server.handle({"jsonrpc": "2.0", "id": 5, "result": {}}) is None


def test_tool_calls_validate_names_and_arguments(tmp_path: Path) -> None:
    server = make_server(tmp_path, 43011)

    unknown = rpc(server, "tools/call", {"name": "fo_missing"})
    extra = rpc(server, "tools/call", {"name": "fo_status", "arguments": {"token": "secret"}})
    wrong_type = rpc(server, "tools/call", {"name": "fo_wait_command", "arguments": {"commandSeq": "1"}})
    missing = rpc(server, "tools/call", {"name": "fo_act", "arguments": {}})
    bad_arguments = rpc(server, "tools/call", {"name": "fo_status", "arguments": [1]})

    assert unknown["error"]["code"] == -32602
    assert "token" in extra["error"]["message"]
    assert extra["error"]["code"] == -32602
    assert wrong_type["error"]["code"] == -32602
    assert "command is required" in missing["error"]["message"]
    assert bad_arguments["error"]["code"] == -32602


def test_serve_loop_recovers_from_bad_frames(tmp_path: Path) -> None:
    server = make_server(tmp_path, 43011)
    oversized = b'{"jsonrpc":"2.0","id":9,"method":"ping","pad":"' + b"x" * 2048 + b'"}\n'
    stream = io.BytesIO(
        b'{"jsonrpc":"2.0","id":1,"method":"ping"}\n'
        + b"\n"
        + b"\xff\xfe\n"
        + oversized
        + b'{"jsonrpc":"2.0","method":"notifications/initialized"}\r\n'
        + b'{"jsonrpc":"2.0","id":2,"method":"ping"}\n'
        + b'{"jsonrpc":"2.0","id":3,"method":"ping"}'
    )
    output = io.BytesIO()

    server.serve(stream, output, 1024)

    responses = [json.loads(line) for line in output.getvalue().splitlines()]
    assert [response.get("id") for response in responses] == [1, None, None, 2, None]
    assert responses[1]["error"]["code"] == -32700
    assert responses[2]["error"]["code"] == -32600
    assert responses[4]["error"]["code"] == -32700
    assert "not newline terminated" in responses[4]["error"]["message"]


def test_read_frame_accepts_exact_limit_and_rejects_one_byte_more() -> None:
    assert read_frame(io.BytesIO(b"a" * 1024 + b"\n"), 1024) == b"a" * 1024

    stream = io.BytesIO(b"a" * 1025 + b"\nnext\n")
    with pytest.raises(Exception, match="exceeds 1024 bytes"):
        read_frame(stream, 1024)
    assert read_frame(stream, 1024) == b"next"
    assert read_frame(stream, 1024) is None


def test_internal_handler_errors_keep_the_session_alive(tmp_path: Path) -> None:
    registry = ToolRegistry(prefix="fo_")
    registry.add_tool("broken", lambda context, arguments: 1 / 0, description="Always fails")
    server = make_server(tmp_path, 43011, registry=registry)

    response = rpc(server, "tools/call", {"name": "broken"})

    assert response["error"]["code"] == -32603
    assert "ZeroDivisionError" in response["error"]["message"]
    assert rpc(server, "ping")["result"] == {}


# Registration extension point


def test_core_tool_set_follows_the_protocol_model_and_prefix(tmp_path: Path) -> None:
    server = make_server(tmp_path, 43011)
    names = [entry["name"] for entry in rpc(server, "tools/list")["result"]["tools"]]

    assert names == ["fo_" + name for name in CORE_TOOLS]

    renamed = ToolRegistry(prefix="game_")
    register_core_tools(renamed)
    assert [entry.name for entry in renamed.tools()][0] == "game_status"

    with pytest.raises(ValueError, match="tool prefix"):
        ToolRegistry(prefix="bad prefix")


def test_registry_rejects_duplicates_and_unsupported_schemas() -> None:
    registry = ToolRegistry(prefix="fo_")
    register_core_tools(registry)

    with pytest.raises(ValueError, match="already registered"):
        registry.add_tool("fo_observe", lambda context, arguments: {}, description="Duplicate")
    with pytest.raises(ValueError, match="cannot replace"):
        registry.add_tool("fo_unknown", lambda context, arguments: {}, description="Missing", replace=True)
    with pytest.raises(ValueError, match="unsupported schema keywords: oneOf"):
        registry.add_tool("game_x", lambda context, arguments: {}, description="X", input_schema={"type": "object", "oneOf": []})
    with pytest.raises(ValueError, match="tool name"):
        registry.add_tool("game x", lambda context, arguments: {}, description="X")
    with pytest.raises(ValueError, match="description"):
        registry.add_tool("game_y", lambda context, arguments: {}, description=" ")

    replaced = registry.add_tool("fo_observe", lambda context, arguments: {"own": True}, description="Project observe", replace=True)
    assert registry.get_tool("fo_observe") is replaced


def test_validate_arguments_covers_the_supported_subset() -> None:
    schema = object_schema(
        {
            "count": {"type": "integer", "minimum": 1, "maximum": 3},
            "mode": {"type": "string", "enum": ["a", "b"]},
            "items": {"type": "array", "items": {"type": "string", "minLength": 1}, "maxItems": 2},
            "flag": {"type": "boolean"},
        },
        ["count"],
    )

    validate_arguments(schema, {"count": 2, "mode": "a", "items": ["x"], "flag": False})

    for value, message in [
        ({}, "count is required"),
        ({"count": True}, "count must be integer"),
        ({"count": 4}, "<= 3"),
        ({"count": 1, "mode": "c"}, "one of"),
        ({"count": 1, "items": [""]}, r"items\[0\]"),
        ({"count": 1, "items": ["a", "b", "c"]}, "at most 2 items"),
        ({"count": 1, "other": 1}, "not a known argument"),
    ]:
        with pytest.raises(InvalidArguments, match=message):
            validate_arguments(schema, value)


PROJECT_MODULE = textwrap.dedent(
    '''
    from ai_control_mcp.registry import object_schema
    from ai_control_mcp.schemas import act_option_properties, endpoint_target_properties


    def register(registry):
        registry.set_readiness(fields=["ready"])
        registry.set_instructions("Call game_move to walk.")
        registry.add_resource("game://guide", lambda context: "# Guide", name="guide", description="Operator guide")

        @registry.tool(
            "game_move",
            description="Walk the agent to a grid cell and wait for the move to finish.",
            input_schema=object_schema(
                {**endpoint_target_properties(), **act_option_properties(), "x": {"type": "integer"}, "y": {"type": "integer"}},
                ["x", "y"],
            ),
        )
        def game_move(context, arguments):
            options = {"waitForCompletion": True, **arguments}
            return context.act(options, {"type": "move", "x": arguments["x"], "y": arguments["y"]})
    '''
)


def test_project_module_registers_tools_from_a_file_and_a_dotted_name(tmp_path: Path, bridge: SampleBridge) -> None:
    package = tmp_path / "game_tools"
    package.mkdir()
    (package / "__init__.py").write_text("", encoding="utf-8")
    (package / "mcp_tools.py").write_text(PROJECT_MODULE, encoding="utf-8")
    module_file = tmp_path / "file_tools.py"
    module_file.write_text(PROJECT_MODULE, encoding="utf-8")

    for spec, search in ((str(module_file), []), ("game_tools.mcp_tools", [tmp_path])):
        registry = ToolRegistry(prefix="fo_")
        register_core_tools(registry)
        register_project_module(registry, load_project_module(spec, search))
        server = make_server(tmp_path, bridge.port, registry=registry)

        moved = tool_ok(server, "game_move", {"x": 4, "y": 5})
        ready = tool_ok(server, "fo_wait_ready", {"timeoutMs": 1000})
        resource = rpc(server, "resources/read", {"uri": "game://guide"})["result"]

        assert moved["completion"]["event"]["message"] == "moved"
        assert ready["ready"] is True
        assert resource["contents"][0]["text"] == "# Guide"


def test_project_module_errors_are_explicit(tmp_path: Path) -> None:
    no_register = tmp_path / "no_register.py"
    no_register.write_text("VALUE = 1\n", encoding="utf-8")
    registry = ToolRegistry(prefix="fo_")

    with pytest.raises(ProjectModuleError, match="must define register"):
        register_project_module(registry, load_project_module(str(no_register), []))
    with pytest.raises(ProjectModuleError, match="not found"):
        load_project_module(str(tmp_path / "missing.py"), [])
    with pytest.raises(ProjectModuleError, match="was not found"):
        load_project_module("missing_game_tools_package", [tmp_path])
    with pytest.raises(ProjectModuleError, match="dotted module name"):
        load_project_module("bad-name", [])


def run_cli(arguments: list[str], env: dict[str, str] | None = None, stdin: str = "") -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(ADAPTER), *arguments],
        input=stdin,
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=60,
        env={**os.environ, **(env or {})},
    )


def test_cli_lists_project_tools_and_reports_configuration_errors(tmp_path: Path) -> None:
    module_file = tmp_path / "file_tools.py"
    module_file.write_text(PROJECT_MODULE, encoding="utf-8")
    broken = tmp_path / "broken.py"
    broken.write_text("def register(registry):\n    registry.add_tool('fo_status', lambda c, a: {}, description='dup')\n", encoding="utf-8")

    listed = run_cli(["--list-tools", "--project-module", str(module_file), "--project-root", str(tmp_path)])
    duplicate = run_cli(["--list-tools", "--project-module", str(broken)])
    remote = run_cli(["--list-tools", "--host", "192.0.2.10"])
    remote_without_token = run_cli(["--list-tools", "--host", "192.0.2.10", "--allow-remote"], {"FONLINE_AI_TOKEN": ""})
    remote_with_token = run_cli(["--list-tools", "--host", "192.0.2.10", "--allow-remote"], {"FONLINE_AI_TOKEN": "secret"})
    bad_profile = tmp_path / "profile.json"
    bad_profile.write_text('{"schemaVersion": 1}', encoding="utf-8")
    profile = run_cli(["--list-tools", "--launch-profile", str(bad_profile)])

    assert listed.returncode == 0, listed.stderr
    assert "game_move" in [entry["name"] for entry in json.loads(listed.stdout)]
    assert duplicate.returncode == 2 and "already registered" in duplicate.stderr
    assert remote.returncode == 2 and "--allow-remote" in remote.stderr
    assert remote_without_token.returncode == 2 and "non-empty token" in remote_without_token.stderr
    assert remote_with_token.returncode == 0, remote_with_token.stderr
    assert profile.returncode == 2 and "devName or an explicit roles" in profile.stderr


def test_cli_never_accepts_a_raw_token_argument() -> None:
    help_text = cli.create_parser().format_help()

    assert "--token-env" in help_text
    assert "--token " not in help_text

    with pytest.raises(SystemExit):
        with patch("sys.stderr", io.StringIO()):
            cli.create_parser().parse_args(["--token", "secret"])
    with pytest.raises(runner.McpRunnerError, match="environment variable"):
        runner.build_adapter_command(["--token=secret"])


def test_cli_serves_stdio_until_end_of_input(tmp_path: Path, bridge: SampleBridge) -> None:
    requests = "".join(
        json.dumps(message) + "\n"
        for message in (
            {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2025-11-25"}},
            {"jsonrpc": "2.0", "method": "notifications/initialized"},
            {"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {"name": "lf_ping", "arguments": {}}},
        )
    )

    completed = run_cli(["--port", str(bridge.port), "--tool-prefix", "lf_", "--project-root", str(tmp_path)], stdin=requests)
    responses = [json.loads(line) for line in completed.stdout.splitlines()]

    assert completed.returncode == 0, completed.stderr
    assert [response["id"] for response in responses] == [1, 2]
    assert json.loads(responses[1]["result"]["content"][0]["text"]) == {"ok": True}


# Session, cursors and command lifecycle against the Engine sample


def test_protocol_tools_round_trip_through_the_sample(tmp_path: Path, bridge: SampleBridge) -> None:
    server = make_server(tmp_path, bridge.port)

    assert tool_ok(server, "fo_ping") == {"ok": True}
    assert tool_ok(server, "fo_status")["maxQueuedCommands"] == 16
    assert tool_ok(server, "fo_observe")["observation"]["schemaVersion"] == 1

    moved = tool_ok(server, "fo_act", {"command": {"type": "move", "x": 2, "y": 3}, "syncAfterCompletion": True})
    failed = tool_ok(server, "fo_act", {"command": {"type": "fail"}, "waitForCompletion": True})

    assert moved["accepted"] is True
    assert moved["completion"]["completed"] is True
    assert moved["completion"]["event"]["commandSeq"] == moved["commandSeq"]
    assert moved["sync"]["observation"]["agent"] == {"x": 2, "y": 3}
    assert failed["completion"]["event"]["success"] is False
    runner.verify_command_payload(moved, "move")

    with pytest.raises(runner.McpRunnerError, match="sample_failure"):
        runner.verify_command_payload(failed, "fail")


def test_event_cursor_advances_resets_and_seeks(tmp_path: Path, bridge: SampleBridge) -> None:
    server = make_server(tmp_path, bridge.port)

    for _ in range(3):
        tool_ok(server, "fo_act", {"command": {"type": "echo"}, "waitForCompletion": True, "includeEvents": False})

    first = tool_ok(server, "fo_next_events", {"reset": True, "limit": 2})
    second = tool_ok(server, "fo_next_events", {"limit": 2})
    raw = tool_ok(server, "fo_events", {"afterSeq": 0, "limit": 500})
    idle = tool_ok(server, "fo_next_events")
    tool_ok(server, "fo_act", {"command": {"type": "echo"}})
    seek = tool_ok(server, "fo_next_events", {"seekLatest": True, "reset": True})

    assert [entry["seq"] for entry in first["events"]] == [1, 2]
    assert first["cursor"] == 2
    assert [entry["seq"] for entry in second["events"]] == [3]
    assert len(raw["events"]) == 3
    assert idle["events"] == [] and idle["cursor"] == 3
    assert seek["events"] == [] and seek["cursor"] == seek["latestSeq"]


def test_wait_command_times_out_without_a_protocol_error(tmp_path: Path) -> None:
    stalled = SampleBridge(run_loop=False)

    try:
        server = make_server(tmp_path, stalled.port)
        accepted = tool_ok(server, "fo_act", {"command": {"type": "echo"}, "waitForCompletion": True, "timeoutMs": 200})
    finally:
        stalled.close()

    assert accepted["accepted"] is True
    assert accepted["completion"]["completed"] is False
    assert accepted["completion"]["timedOut"] is True

    with pytest.raises(runner.McpRunnerError, match="timeout"):
        runner.verify_command_payload(accepted, "echo")


def test_wait_command_truncates_and_keeps_trailing_batch_events(tmp_path: Path, bridge: SampleBridge) -> None:
    server = make_server(tmp_path, bridge.port)
    first = tool_ok(server, "fo_act", {"command": {"type": "echo"}})
    tool_ok(server, "fo_act", {"command": {"type": "echo"}})
    tool_ok(server, "fo_act", {"command": {"type": "echo"}})
    time.sleep(0.3)

    waited = tool_ok(server, "fo_wait_command", {"commandSeq": first["commandSeq"], "maxReturnedEvents": 2})

    assert waited["completed"] is True
    assert [entry["seq"] for entry in waited["events"]] == [1, 2]
    assert waited["eventsTruncated"] is True
    assert waited["cursor"] == 3


def test_completion_must_be_newer_than_the_pre_act_baseline(tmp_path: Path) -> None:
    # The stale entry carries the same commandSeq and arrived before act; only the later one counts
    stale = {"seq": 5, "event": {"type": "command_completed", "commandSeq": 7, "success": False, "message": "stale"}}
    fresh = {"seq": 6, "event": {"type": "command_completed", "commandSeq": 7, "success": True, "message": "fresh"}}
    phase = {"acted": False}

    def respond(request: dict[str, Any]) -> bytes:
        if request["method"] == "act":
            phase["acted"] = True
            return reply(request, {"accepted": True, "commandSeq": 7})
        if request["method"] == "events":
            after = request["params"]["afterSeq"]
            retained = [stale, fresh] if phase["acted"] else [stale]
            entries = [entry for entry in retained if entry["seq"] > after]
            return reply(request, {"latestSeq": retained[-1]["seq"], "events": entries})
        return reply(request, {})

    peer = ScriptedPeer(respond)

    try:
        server = make_server(tmp_path, peer.port)
        payload = tool_ok(server, "fo_act", {"command": {"type": "echo"}, "waitForCompletion": True, "timeoutMs": 1000})
    finally:
        peer.close()

    assert payload["completion"]["event"]["message"] == "fresh"


def test_restarted_bridge_history_resets_the_cursor(tmp_path: Path) -> None:
    latest = {"value": 9}

    def respond(request: dict[str, Any]) -> bytes:
        after = request["params"].get("afterSeq", 0)
        entries = [{"seq": seq, "event": {"type": "tick"}} for seq in range(after + 1, latest["value"] + 1)]
        return reply(request, {"latestSeq": latest["value"], "events": entries})

    peer = ScriptedPeer(respond)

    try:
        server = make_server(tmp_path, peer.port)
        before = tool_ok(server, "fo_next_events")
        latest["value"] = 2
        restarted = tool_ok(server, "fo_next_events")
        after = tool_ok(server, "fo_next_events")
    finally:
        peer.close()

    assert before["cursor"] == 9
    assert restarted["historyReset"] is True and restarted["cursor"] == 0
    assert [entry["seq"] for entry in after["events"]] == [1, 2]


@pytest.mark.parametrize(
    ("method", "result", "message"),
    [
        ("events", {"events": []}, "latestSeq"),
        ("events", {"latestSeq": 3, "events": [{"seq": 2, "event": {}}, {"seq": 1, "event": {}}]}, "ascending"),
        ("events", {"latestSeq": 3, "events": [{"seq": "1", "event": {}}]}, "integer seq"),
        ("act", {"accepted": False, "commandSeq": 1}, "accepted=true"),
        ("observe", {"observationSeq": 1}, "observation object"),
        ("status", [1, 2], "must be an object"),
    ],
)
def test_malformed_peer_results_become_tool_errors(tmp_path: Path, method: str, result: Any, message: str) -> None:
    peer = ScriptedPeer(lambda request: reply(request, result))
    tool_name = {"events": "fo_events", "act": "fo_act", "observe": "fo_observe", "status": "fo_status"}[method]
    arguments = {"command": {"type": "echo"}} if method == "act" else {}

    try:
        payload, is_error = tool(make_server(tmp_path, peer.port), tool_name, arguments)
    finally:
        peer.close()

    assert is_error
    assert payload["error"]["type"] == "ProtocolError"
    assert message in payload["error"]["message"]


def test_silent_peer_times_out_and_the_next_call_reconnects(tmp_path: Path) -> None:
    answers: queue.Queue[bool] = queue.Queue()
    answers.put(False)

    def respond(request: dict[str, Any]) -> bytes | None:
        try:
            answer = answers.get_nowait()
        except queue.Empty:
            answer = True

        if not answer:
            time.sleep(1.0)
            return reply(request, {"ok": "late"})

        return reply(request, {"ok": True})

    peer = ScriptedPeer(respond)

    try:
        server = make_server(tmp_path, peer.port, timeout=0.3)
        timed_out, timed_out_error = tool(server, "fo_ping")
        recovered = tool_ok(server, "fo_ping")
    finally:
        peer.close()

    assert timed_out_error
    assert recovered == {"ok": True}


def test_remote_errors_and_unreachable_bridges_are_tool_errors(tmp_path: Path) -> None:
    peer = ScriptedPeer(
        lambda request: json.dumps({"jsonrpc": "2.0", "id": request["id"], "error": {"code": -32002, "message": "Command queue full"}}).encode() + b"\n"
    )

    try:
        full, full_error = tool(make_server(tmp_path, peer.port), "fo_act", {"command": {"type": "echo"}})
    finally:
        peer.close()

    unreachable, unreachable_error = tool(make_server(tmp_path, free_port(), timeout=0.5), "fo_ping")

    assert full_error and full["error"]["code"] == -32002
    assert unreachable_error and unreachable["error"]["type"] in {"ConnectionRefusedError", "TimeoutError", "OSError"}


def test_token_authorizes_connections_and_never_appears_in_results(tmp_path: Path) -> None:
    secured = SampleBridge(token="bridge-secret-value")

    try:
        good = make_server(tmp_path, secured.port, token="bridge-secret-value")
        bad = make_server(tmp_path, secured.port, token="wrong-token")
        anonymous = make_server(tmp_path, secured.port)
        ping = tool_ok(good, "fo_ping")
        listing = tool_ok(good, "fo_endpoints", {"probe": True})
        rejected, rejected_error = tool(bad, "fo_ping")
        unauthorized, unauthorized_error = tool(anonymous, "fo_ping")
    finally:
        secured.close()

    assert ping == {"ok": True}
    assert listing["tokenConfigured"] is True and listing["endpoints"][0]["ready"] is True
    assert "bridge-secret-value" not in json.dumps(listing)
    assert rejected_error and rejected["error"]["type"] == "AuthorizationError"
    assert unauthorized_error and unauthorized["error"]["code"] == -32001


def test_connect_enforces_loopback_and_remote_token_rules(tmp_path: Path, bridge: SampleBridge) -> None:
    local = make_server(tmp_path, 43011)
    remote_opt_in_without_token = make_server(tmp_path, 43011, allow_remote=True)

    connected = tool_ok(local, "fo_connect", {"host": "127.0.0.1", "port": bridge.port})
    refused = rpc(local, "tools/call", {"name": "fo_connect", "arguments": {"host": "192.0.2.10", "port": 43011}})
    tokenless = rpc(remote_opt_in_without_token, "tools/call", {"name": "fo_connect", "arguments": {"host": "192.0.2.10", "port": 43011}})
    mixed = rpc(local, "tools/call", {"name": "fo_connect", "arguments": {"host": "127.0.0.1", "port": 1, "endpointId": "default"}})

    assert connected["selectedEndpoint"]["port"] == bridge.port and connected["ping"] == {"ok": True}
    assert tool_ok(local, "fo_ping") == {"ok": True}
    assert "--allow-remote" in refused["error"]["message"]
    assert "non-empty token" in tokenless["error"]["message"]
    assert mixed["error"]["code"] == -32602

    with pytest.raises(InvalidArguments, match="--allow-remote"):
        Session(host="192.0.2.10", port=43011, token="t", timeout=1.0, allow_remote=False)


def test_wait_ready_uses_the_project_gate_and_reports_missing_fields(tmp_path: Path, bridge: SampleBridge) -> None:
    server = make_server(tmp_path, bridge.port)

    default_gate = tool_ok(server, "fo_wait_ready", {"timeoutMs": 500})
    explicit = tool_ok(server, "fo_wait_ready", {"requireFields": ["ready", "agent"], "includeObservation": False})
    missing = tool_ok(server, "fo_wait_ready", {"requireFields": ["inWorld"], "timeoutMs": 300, "pollIntervalMs": 50})
    server.registry.set_readiness(lambda observation, arguments: [] if observation["agent"]["x"] >= 0 else ["agent"])
    custom = tool_ok(server, "fo_wait_ready", {"timeoutMs": 300})
    all_ready = tool_ok(server, "fo_wait_all_ready", {"endpointIds": ["default"], "timeoutMs": 300})

    assert default_gate["ready"] is True and default_gate["missing"] == []
    assert explicit["ready"] is True and "observation" not in explicit
    assert missing["ready"] is False and missing["timedOut"] is True and missing["missing"] == ["inWorld"]
    assert missing["attempts"] >= 2
    assert custom["ready"] is True
    assert all_ready["ready"] is True and all_ready["endpoints"][0]["attempts"] == 1


def test_wait_ready_keeps_polling_while_the_bridge_is_not_listening(tmp_path: Path) -> None:
    payload = tool_ok(make_server(tmp_path, free_port(), timeout=0.2), "fo_wait_ready", {"timeoutMs": 400, "pollIntervalMs": 50})

    assert payload["ready"] is False and payload["timedOut"] is True
    assert payload["missing"] == ["bridge"] and payload["attempts"] >= 2


def test_observe_fresh_waits_for_a_newer_snapshot(tmp_path: Path) -> None:
    sequence = {"value": 1, "advance": True}

    def respond(request: dict[str, Any]) -> bytes:
        current = sequence["value"]

        if sequence["advance"]:
            sequence["value"] += 1

        return reply(request, {"observationSeq": current, "observation": {"schemaVersion": 1}})

    peer = ScriptedPeer(respond)

    try:
        server = make_server(tmp_path, peer.port)
        fresh = tool_ok(server, "fo_observe", {"fresh": True, "freshTimeoutMs": 1000})
        sequence["advance"] = False
        stale = tool_ok(server, "fo_observe", {"fresh": True, "freshTimeoutMs": 250})
    finally:
        peer.close()

    assert fresh["observationFresh"] is True and fresh["observationSeq"] == 2
    assert stale["observationFresh"] is False


# Launch profiles and orchestration


BRIDGE_SETTINGS = {
    "enabledSetting": "Bridge.Enabled",
    "hostSetting": "Bridge.Host",
    "portSetting": "Bridge.Port",
    "tokenSetting": "Bridge.Token",
    "portStrideSetting": "Bridge.PortStride",
}


def test_launch_profile_derives_standard_roles_and_rejects_bad_input() -> None:
    profile = parse_launch_profile({"schemaVersion": 1, "devName": "MyGame", "config": "MyGame.fomain", "bridge": BRIDGE_SETTINGS})

    assert profile.roles["server_headless"].binary == "MyGame_ServerHeadless"
    assert profile.roles["client"].endpoints == "client"
    assert profile.default_role == "server"

    for raw, message in [
        ({"schemaVersion": 2, "devName": "G"}, "schemaVersion"),
        ({"schemaVersion": 1, "devName": "G", "extra": 1}, "unknown keys: extra"),
        ({"schemaVersion": 1, "roles": {"client": {"target": "Client"}}}, "needs the profile devName"),
        ({"schemaVersion": 1, "roles": {"client": {"binary": "a", "target": "b"}}}, "exactly one of"),
        ({"schemaVersion": 1, "roles": {"client": {"binary": "bin/a"}}}, "without directories"),
        ({"schemaVersion": 1, "roles": {"Client": {"binary": "a"}}}, "role name"),
        ({"schemaVersion": 1, "roles": {"client": {"binary": "a", "endpoints": "many"}}}, "endpoints must be"),
        ({"schemaVersion": 1, "devName": "G", "bridge": {"enabledSetting": "Bridge.Enabled"}}, "needs hostSetting"),
        ({"schemaVersion": 1, "devName": "G", "binarySearchDirs": ["../outside"]}, "without '..'"),
        ({"schemaVersion": 1, "devName": "G", "binaryEnvTemplate": "BAD-NAME"}, "binaryEnvTemplate"),
        ({"schemaVersion": 1, "devName": "G", "defaultRole": "editor"}, "defaultRole"),
        ({"schemaVersion": 1, "roles": {"client": {"binary": "a", "settings": {"Bad": 1}}}}, "Group.Name"),
    ]:
        with pytest.raises(LaunchProfileError, match=message):
            parse_launch_profile(raw)


def test_launch_settings_wire_the_bridge_without_a_raw_token(tmp_path: Path) -> None:
    profile = parse_launch_profile(
        {
            "schemaVersion": 1,
            "devName": "MyGame",
            "bridge": BRIDGE_SETTINGS,
            "roles": {
                "server_headless": {"target": "ServerHeadless", "endpoints": "embedded-clients", "defaultClientCount": 1},
                "client": {"target": "Client", "endpoints": "client", "settings": {"Game.Profile": "qa"}},
            },
        }
    )
    session = Session(host="127.0.0.1", port=45000, token="secret-token", timeout=1.0, allow_remote=False)
    orchestrator = Orchestrator(tmp_path, session, profile, token_env="GAME_AI_TOKEN")

    settings, plan = orchestrator.launch_settings(profile, profile.roles["server_headless"], {"clientCount": 3, "bridgePortStride": 10})
    client_settings, client_plan = orchestrator.launch_settings(profile, profile.roles["client"], {"settings": {"Game.Profile": "dev"}})
    no_bridge, no_plan = orchestrator.launch_settings(profile, profile.roles["client"], {"bridge": False})

    assert settings["Server.AutoStartClientOnServer"] == 3
    assert settings["Bridge.Token"] == "$TARGET_ENV{GAME_AI_TOKEN}"
    assert "secret-token" not in json.dumps(settings)
    assert settings["Common.NonInteractive"] is True
    assert plan == [("127.0.0.1", 45000), ("127.0.0.1", 45010), ("127.0.0.1", 45020)]
    assert client_settings["Game.Profile"] == "dev" and client_plan == [("127.0.0.1", 45000)]
    assert "Bridge.Enabled" not in no_bridge and no_plan == []

    for arguments, message in [
        ({"settings": {"Bridge.Token": "raw"}}, "must not set Bridge.Token"),
        ({"settings": {"Game.List": [1]}}, "string, number or boolean"),
        ({"bridgeHost": "192.0.2.10"}, "--allow-remote"),
        ({"bridgePort": 70000}, "between 1 and 65535"),
        ({"clientCount": -1}, "non-negative"),
    ]:
        with pytest.raises(InvalidArguments, match=message):
            orchestrator.launch_settings(profile, profile.roles["server_headless"], arguments)

    no_stride = parse_launch_profile({"schemaVersion": 1, "devName": "G", "bridge": {**BRIDGE_SETTINGS, "portStrideSetting": None}})
    with pytest.raises(InvalidArguments, match="portStrideSetting"):
        Orchestrator(tmp_path, session, no_stride, token_env="T").launch_settings(no_stride, no_stride.roles["server"], {"clientCount": 2})


def test_format_setting_value_keeps_engine_command_line_semantics() -> None:
    assert format_setting_value(True) == "True"
    assert format_setting_value(False) == "False"
    assert format_setting_value(7) == "7"
    assert format_setting_value("-1") == " -1"
    assert format_setting_value("$TARGET_ENV{X}") == "$TARGET_ENV{X}"


def test_binary_resolution_prefers_the_newest_build_and_stays_inside_the_root(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    suffix = ".exe" if os.name == "nt" else ""
    old = root / "Binaries" / "old" / f"G_Client{suffix}"
    new = root / "Build" / "new" / f"G_Client{suffix}"

    for path, stamp in ((old, 1_000_000), (new, 2_000_000)):
        path.parent.mkdir(parents=True)
        path.write_text("", encoding="utf-8")
        os.utime(path, (stamp, stamp))

    profile = parse_launch_profile({"schemaVersion": 1, "devName": "G", "binaryEnvTemplate": "G_AI_{ROLE}_EXE"})
    orchestrator = Orchestrator(root, Session(host="127.0.0.1", port=1, token="", timeout=1.0, allow_remote=False), profile, token_env="T")

    assert orchestrator.resolve_binary(profile, profile.roles["client"], None) == new
    assert orchestrator.resolve_binary(profile, profile.roles["client"], str(old.relative_to(root))) == old

    with patch.dict(os.environ, {"G_AI_CLIENT_EXE": str(old)}):
        assert orchestrator.resolve_binary(profile, profile.roles["client"], None) == old
    with pytest.raises(InvalidArguments, match="inside the project root"):
        orchestrator.resolve_binary(profile, profile.roles["client"], str(root.parent / "elsewhere.exe"))
    with pytest.raises(InvalidArguments, match="no G_Server"):
        orchestrator.resolve_binary(profile, profile.roles["server"], None)


def test_launch_options_lists_roles_and_sub_configs(tmp_path: Path) -> None:
    (tmp_path / "G.fomain").write_text(
        "Common.Name = G\n\n[SubConfig]\nName = Smoke\n\n[SubConfig]\nName = Qa\nParent = Smoke\n", encoding="utf-8"
    )
    server = make_server(tmp_path, 43011, profile={"schemaVersion": 1, "devName": "G", "config": "G.fomain"})
    unconfigured = make_server(tmp_path, 43011)

    options = tool_ok(server, "fo_launch_options")
    launch = rpc(unconfigured, "tools/call", {"name": "fo_launch", "arguments": {}})

    assert options["subConfigs"] == [{"name": "Smoke", "parent": ""}, {"name": "Qa", "parent": "Smoke"}]
    assert [role["role"] for role in options["roles"]] == ["server", "server_headless", "client", "client_headless", "mapper"]
    assert tool_ok(unconfigured, "fo_launch_options") == {"configured": False, "roles": []}
    assert "launch profile" in launch["error"]["message"]


def test_logs_read_a_bounded_tail_and_reject_paths(tmp_path: Path) -> None:
    (tmp_path / "Game.log").write_text("".join(f"line {index}\n" for index in range(500)), encoding="utf-8")
    server = make_server(tmp_path, 43011, profile={"schemaVersion": 1, "devName": "G"})

    tail = tool_ok(server, "fo_logs", {"logName": "Game.log", "lines": 3})
    window = tool_ok(server, "fo_logs", {"logName": "Game.log", "maxBytes": 1024, "lines": 2000})
    missing = tool_ok(server, "fo_logs", {"logName": "Absent.log"})
    escaped = rpc(server, "tools/call", {"name": "fo_logs", "arguments": {"logName": "..\\secret.log"}})

    assert tail["text"].splitlines() == ["line 497", "line 498", "line 499"]
    assert window["truncatedBytes"] is True and window["text"].splitlines()[0].startswith("line ")
    assert missing["exists"] is False and missing["text"] == ""
    assert "file name" in escaped["error"]["message"]


FAKE_GAME = textwrap.dedent(
    """
    import json, os, re, sys, threading, time
    from pathlib import Path

    sys.path.insert(0, {sample!r})
    sys.path.insert(0, {build_tools!r})
    import ai_control_sample as sample

    log_stem, arguments = sys.argv[1], sys.argv[2:]
    settings, index = {{}}, 0

    while index < len(arguments):
        settings[arguments[index].lstrip("-")] = arguments[index + 1].strip()
        index += 2

    Path(log_stem + ".log").write_text(json.dumps(arguments) + "\\nstarted\\n", encoding="utf-8")

    if settings.get("Bridge.Enabled") == "True":
        token = re.sub(r"\\$TARGET_ENV\\{{(\\w+)\\}}", lambda match: os.environ.get(match.group(1), ""), settings.get("Bridge.Token", ""))
        state = sample.SampleState(token, 16, 64)
        server = sample.AiControlServer((settings["Bridge.Host"], int(settings["Bridge.Port"])), state)
        threading.Thread(target=sample._game_loop, args=(state,), daemon=True).start()
        server.serve_forever(poll_interval=0.05)
    else:
        time.sleep(120)
    """
)


def write_fake_game(root: Path, binary: str) -> Path:
    script = root / "fake_game.py"
    script.write_text(FAKE_GAME.format(sample=str(SAMPLE_DIR), build_tools=str(BUILD_TOOLS)), encoding="utf-8")

    if os.name == "nt":
        wrapper = root / "Binaries" / f"{binary}.cmd"
        wrapper.parent.mkdir(parents=True, exist_ok=True)
        wrapper.write_text(f'@echo off\r\n"{sys.executable}" -I "{script}" {binary} %*\r\n', encoding="utf-8")
    else:
        wrapper = root / "Binaries" / binary
        wrapper.parent.mkdir(parents=True, exist_ok=True)
        wrapper.write_text(f'#!/bin/sh\nexec "{sys.executable}" -I "{script}" {binary} "$@"\n', encoding="utf-8")
        wrapper.chmod(0o755)

    return wrapper


@pytest.mark.skipif(not OWNERSHIP_PLATFORM, reason="owned launches are supported on Windows and Linux")
def test_launch_hands_the_token_through_the_environment_and_stops_the_tree(tmp_path: Path) -> None:
    root = tmp_path.resolve()
    wrapper = write_fake_game(root, "G_Client")
    profile = {"schemaVersion": 1, "devName": "G", "config": "G.fomain", "bridge": BRIDGE_SETTINGS,
               "roles": {"client": {"target": "Client", "endpoints": "client"}}}
    port = free_port()

    with patch.dict(os.environ, {"FONLINE_AI_TOKEN": "launch-secret"}):
        server = make_server(root, port, token="launch-secret", profile=profile)

        try:
            launched = tool_ok(server, "fo_launch", {"binaryPath": str(wrapper.relative_to(root)), "subConfigs": ["Smoke"],
                                                     "timeoutMs": 20000})
            ready = tool_ok(server, "fo_wait_ready", {"processId": launched["processId"], "timeoutMs": 5000})
            moved = tool_ok(server, "fo_act", {"processId": launched["processId"], "command": {"type": "move", "x": 1},
                                               "waitForCompletion": True})
            logs = tool_ok(server, "fo_logs", {"processId": launched["processId"]})
            processes = tool_ok(server, "fo_processes")["processes"]
        finally:
            stopped = tool_ok(server, "fo_stop_process", {"all": True})

    assert launched["wait"]["ready"] is True, launched
    assert launched["selectedEndpoint"]["endpointId"] == f"p{launched['processId']}:e0"
    assert "-ApplySubConfig" in launched["command"] and "Smoke" in launched["command"]
    assert "$TARGET_ENV{FONLINE_AI_TOKEN}" in launched["command"]
    assert "launch-secret" not in json.dumps(launched)
    assert ready["ready"] is True
    assert moved["completion"]["event"]["message"] == "moved"
    assert logs["exists"] is True and "started" in logs["text"]
    assert processes[0]["running"] is True
    assert stopped["stopped"][0]["processId"] == launched["processId"]
    assert tool_ok(server, "fo_processes")["processes"] == []
    assert [entry["endpointId"] for entry in tool_ok(server, "fo_endpoints")["endpoints"]] == ["default"]


# Process ownership through the real adapter (ported ownership regressions)


LEAF_SCRIPT = """
import os, sys, time
with open(sys.argv[1], "w", encoding="utf-8") as handle:
    handle.write(str(os.getpid()))
time.sleep(120)
"""


def local_process_alive(pid: int) -> bool:
    if os.name == "posix":
        try:
            os.kill(pid, 0)
            return True
        except ProcessLookupError:
            return False
        except PermissionError:
            return True

    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    handle = kernel32.OpenProcess(0x1000, False, pid)

    if not handle:
        return False

    try:
        exit_code = wintypes.DWORD()
        return bool(kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code))) and exit_code.value == 259
    finally:
        kernel32.CloseHandle(handle)


def wait_for_exit(pid: int, timeout: float = 15.0) -> bool:
    deadline = time.monotonic() + timeout

    while time.monotonic() < deadline:
        if not local_process_alive(pid):
            return True

        time.sleep(0.1)

    return not local_process_alive(pid)


def kill_by_pid(pid: int) -> None:
    # Test cleanup of one known leaf PID that this test created; never by image name
    if os.name == "posix":
        try:
            os.kill(pid, 9)
        except ProcessLookupError:
            pass
    else:
        subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True, check=False)


class OwnershipFixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.clients: list[runner.McpProcess] = []
        self.leaves: list[int] = []
        (root / "profile.json").write_text(
            json.dumps({"schemaVersion": 1, "roles": {"client": {"binary": "sleeper", "endpoints": "none"}}}), encoding="utf-8"
        )

    def make_tree(self, tag: str, *, detached: bool = False, parent_exits: bool = False, nested: bool = True) -> tuple[Path, Path]:
        pid_file = self.root / f"leaf_{tag}.pid"
        leaf = self.root / f"leaf_{tag}.py"
        leaf.write_text(LEAF_SCRIPT.replace("sys.argv[1]", repr(str(pid_file))), encoding="utf-8")
        target = leaf

        if nested:
            parent = self.root / f"parent_{tag}.py"
            parent.write_text(
                "import subprocess,sys,time\nfrom pathlib import Path\n"
                f"child=subprocess.Popen([sys.executable,{str(leaf)!r}],start_new_session={detached!r})\n"
                f"pid=Path({str(pid_file)!r})\n"
                "while not pid.exists() or not pid.read_text().strip():time.sleep(0.01)\n"
                + ("" if parent_exits else "child.wait()\n"),
                encoding="utf-8",
            )
            target = parent

        if os.name == "nt":
            wrapper = self.root / f"sleeper_{tag}.cmd"
            wrapper.write_text(f'@echo off\r\n"{sys.executable}" -I "{target}"\r\n', encoding="utf-8")
        else:
            wrapper = self.root / f"sleeper_{tag}"
            wrapper.write_text(f'#!/bin/sh\nexec "{sys.executable}" -I "{target}"\n', encoding="utf-8")
            wrapper.chmod(0o755)

        return wrapper, pid_file

    def spawn(self, extra: list[str] | None = None, guardian_code: str | None = None) -> runner.McpProcess:
        arguments = ["--project-root", str(self.root), "--launch-profile", str(self.root / "profile.json"), *(extra or [])]
        command = runner.build_adapter_command(arguments)

        if guardian_code is not None:
            setup = (
                f"import sys;sys.path.insert(0,{str(BUILD_TOOLS)!r});"
                "from ai_control_mcp import process_owner as owner;from ai_control_mcp import cli;"
                f"owner.guardian_command=lambda:[sys.executable,'-u','-c',{guardian_code!r}];"
                "raise SystemExit(cli.main())"
            )
            command = [sys.executable, "-u", "-c", setup, *arguments]

        client = runner.McpProcess(command, request_timeout=30.0, env={**os.environ, "FONLINE_AI_TOKEN": ""})
        self.clients.append(client)
        runner.initialize_client(client, "ownership-regression")
        return client

    def launch(self, client: runner.McpProcess, wrapper: Path) -> dict[str, Any]:
        return runner.call_tool(client, "fo_launch", {"role": "client", "binaryPath": str(wrapper), "bridge": False})

    def wait_pid(self, pid_file: Path, timeout: float = 15.0) -> int:
        deadline = time.monotonic() + timeout

        while time.monotonic() < deadline:
            if pid_file.exists() and pid_file.read_text(encoding="utf-8").strip():
                pid = int(pid_file.read_text(encoding="utf-8"))
                self.leaves.append(pid)
                return pid

            time.sleep(0.05)

        raise AssertionError(f"leaf did not write {pid_file}")

    def close(self) -> None:
        for client in self.clients:
            if client.process.poll() is None:
                client.close()

        for pid in self.leaves:
            if local_process_alive(pid):
                kill_by_pid(pid)


@pytest.fixture
def ownership(tmp_path: Path) -> Iterator[OwnershipFixture]:
    if not OWNERSHIP_PLATFORM:
        pytest.skip("process ownership is supported on Windows and Linux")

    fixture = OwnershipFixture(tmp_path.resolve())
    yield fixture
    fixture.close()


@pytest.mark.parametrize("mode", ["eof", "terminate", "hard"])
def test_adapter_exit_stops_launched_trees_including_descendants(ownership: OwnershipFixture, mode: str) -> None:
    client = ownership.spawn()
    wrapper, pid_file = ownership.make_tree(mode)
    launched = ownership.launch(client, wrapper)
    leaf = ownership.wait_pid(pid_file)

    assert local_process_alive(leaf)

    if mode == "eof":
        client.close()
    elif mode == "terminate":
        client.process.terminate()
        client.process.wait(timeout=10)
    else:
        client.process.kill()
        client.process.wait(timeout=10)

    assert wait_for_exit(int(launched["osPid"]))
    assert wait_for_exit(leaf), f"descendant survived adapter {mode}"


def test_keep_processes_mode_leaves_the_tree_running(ownership: OwnershipFixture) -> None:
    client = ownership.spawn(["--keep-processes-on-exit"])
    wrapper, pid_file = ownership.make_tree("keep", nested=False)
    ownership.launch(client, wrapper)
    leaf = ownership.wait_pid(pid_file)

    client.close()
    time.sleep(1.0)

    assert local_process_alive(leaf)


def test_stop_one_tree_preserves_the_other(ownership: OwnershipFixture) -> None:
    client = ownership.spawn()
    first, first_file = ownership.make_tree("first")
    second, second_file = ownership.make_tree("second")
    first_launch = ownership.launch(client, first)
    ownership.launch(client, second)
    first_leaf = ownership.wait_pid(first_file)
    second_leaf = ownership.wait_pid(second_file)

    runner.call_tool(client, "fo_stop_process", {"processId": first_launch["processId"]})

    assert wait_for_exit(first_leaf)
    assert local_process_alive(second_leaf)

    runner.call_tool(client, "fo_stop_process", {"all": True})

    assert wait_for_exit(second_leaf)
    assert runner.call_tool(client, "fo_processes")["processes"] == []


def test_owned_process_reports_target_pid_and_exit_code(tmp_path: Path) -> None:
    if not OWNERSHIP_PLATFORM:
        pytest.skip("process ownership is supported on Windows and Linux")

    pid_file = tmp_path / "target.pid"
    code = f"import os;from pathlib import Path;Path({str(pid_file)!r}).write_text(str(os.getpid()));raise SystemExit(23)"
    process = process_owner.spawn_managed_process([sys.executable, "-c", code], str(tmp_path), False)

    assert process.wait(timeout=15) == 23
    assert process.pid == int(pid_file.read_text())
    assert process.poll() == 23


@pytest.mark.skipif(not sys.platform.startswith("linux"), reason="Linux child subreaper")
def test_linux_detached_descendant_is_stopped_after_primary_exit(ownership: OwnershipFixture) -> None:
    wrapper, pid_file = ownership.make_tree("detached", detached=True, parent_exits=True)
    process = process_owner.spawn_managed_process([str(wrapper)], str(ownership.root), False)

    assert process.wait(timeout=15) == 0
    assert wait_for_exit(ownership.wait_pid(pid_file))


def test_owner_initialization_failure_rejects_the_launch(ownership: OwnershipFixture) -> None:
    code = (
        f"import sys;sys.path.insert(0,{str(BUILD_TOOLS)!r});"
        "from ai_control_mcp import process_owner as owner\n"
        "def fail():raise OSError('injected protection failure')\n"
        "owner.arm_windows_owner=fail;owner.arm_linux_owner=fail\n"
        "raise SystemExit(owner.guardian_main())"
    )
    client = ownership.spawn(guardian_code=code)
    wrapper, pid_file = ownership.make_tree("failure", nested=False)

    with pytest.raises(runner.McpRunnerError, match="injected protection failure"):
        ownership.launch(client, wrapper)

    assert not pid_file.exists()
    assert runner.call_tool(client, "fo_processes")["processes"] == []


def test_unsupported_platform_rejects_ownership_before_starting_the_target() -> None:
    with patch.object(process_owner.sys, "platform", "darwin"), \
            patch.object(process_owner, "report_status") as report, \
            patch.object(process_owner.subprocess, "Popen") as popen:
        process_owner.guardian_main()

    popen.assert_not_called()
    assert "requires Windows or Linux" in report.call_args.args[0]["error"]


def test_keep_mode_spawns_without_an_owner(tmp_path: Path) -> None:
    with patch.object(process_owner, "OwnedProcess") as owner, patch.object(process_owner.subprocess, "Popen") as popen:
        result = process_owner.spawn_managed_process(["fixture"], str(tmp_path), True)

    assert result is popen.return_value
    owner.assert_not_called()


def test_exit_cleanup_reports_a_stop_failure_without_raising(tmp_path: Path) -> None:
    class FailingOrchestrator:
        def stop_all_for_exit(self) -> None:
            raise RuntimeError("registry refuses to stop")

    stderr = io.StringIO()

    with patch("sys.stderr", stderr):
        cli.stop_owned_processes(FailingOrchestrator())  # type: ignore[arg-type]

    assert "adapter exit process cleanup failed" in stderr.getvalue()


# Runner client and advisory helpers


def test_mcp_process_skips_notifications_and_discards_late_responses() -> None:
    client = runner.McpProcess.__new__(runner.McpProcess)
    client._next_id = 1
    client._request_timeout = 0.001
    client._abandoned_request_ids = set()
    client._stdout_lines = queue.Queue()

    class FakeProcess:
        stdin = io.StringIO()
        returncode = None

        def poll(self) -> None:
            return None

    client._process = FakeProcess()  # type: ignore[assignment]

    with pytest.raises(runner.McpRunnerError, match="response timeout"):
        client.request("tools/call", {"name": "slow"})

    client._stdout_lines.put(json.dumps({"jsonrpc": "2.0", "method": "notifications/message", "params": {}}) + "\n")
    client._stdout_lines.put(json.dumps({"jsonrpc": "2.0", "id": 1, "result": {"late": True}}) + "\n")
    client._stdout_lines.put(json.dumps({"jsonrpc": "2.0", "id": 2, "result": {"ok": True}}) + "\n")
    response = client.request("tools/call", {"name": "fast"}, timeout=1.0)

    assert response["result"] == {"ok": True}
    assert client._abandoned_request_ids == set()

    client._stdout_lines.put(json.dumps({"jsonrpc": "2.0", "id": 99, "result": {}}) + "\n")
    with pytest.raises(runner.McpRunnerError, match="unexpected MCP response id"):
        client.request("ping", timeout=1.0)


def test_runner_drives_the_adapter_process_end_to_end(tmp_path: Path, bridge: SampleBridge) -> None:
    command = runner.build_adapter_command(["--port", str(bridge.port), "--project-root", str(tmp_path)])
    client = runner.McpProcess(command, request_timeout=30.0)

    try:
        info = runner.initialize_client(client, "runner-test")
        payload = runner.call_tool(client, "fo_act", runner.command_args({"endpointId": "default"}, {"type": "echo", "stringArg": "hi"}))
        ready = runner.wait_ready(client, {"endpointId": "default"}, require_fields=["ready"], timeout_ms=2000)

        with pytest.raises(runner.McpRunnerError, match="unknown processId"):
            runner.call_tool(client, "fo_stop_process", {"processId": 5})
        with pytest.raises(runner.McpRunnerError, match="tool error"):
            runner.call_tool(client, "fo_connect", {"host": "127.0.0.1", "port": free_port()})
    finally:
        exit_code = client.close()

    runner.verify_command_payload(payload, "echo")
    assert info["serverInfo"]["name"] == "fonline-ai-control"
    assert payload["completion"]["event"]["message"] == "hi"
    assert ready["ready"] is True
    assert exit_code == 0


def test_report_writer_replaces_files_atomically(tmp_path: Path) -> None:
    target = tmp_path / "reports" / "run.json"

    runner.write_json_report(target, {"status": "passed", "name": "проверка"})
    runner.write_json_report(target, {"status": "failed"})

    assert json.loads(target.read_text(encoding="utf-8")) == {"status": "failed"}
    assert [path.name for path in target.parent.iterdir()] == ["run.json"]


def test_command_args_and_payload_verification() -> None:
    arguments = runner.command_args({"endpointId": "p1:e0", "role": "client"}, {"type": "echo"}, wait=False)

    assert arguments == {
        "endpointId": "p1:e0", "command": {"type": "echo"}, "waitForCompletion": False, "timeoutMs": 20000,
        "pollIntervalMs": 150, "syncAfterCompletion": False, "includeObservation": False,
    }

    runner.verify_command_payload({"accepted": True, "commandSeq": 1}, "queued")

    with pytest.raises(runner.McpRunnerError, match="not accepted"):
        runner.verify_command_payload({"accepted": False}, "rejected")
    with pytest.raises(runner.McpRunnerError, match="history restarted"):
        runner.verify_command_payload({"accepted": True, "completion": {"completed": False, "historyReset": True}}, "reset")


def test_advisory_helpers_rank_and_bound_options() -> None:
    options = [
        advisory.task_option("loot", 140, "near", "game_pick", {"id": 1}),
        advisory.task_option("talk", 50, "quest", "game_talk", {"id": 2}, {"npc": "guard"}),
        advisory.task_option("rest", 50, "safe", "game_rest", {}),
    ]

    assert options[0]["score"] == 100 and options[1]["npc"] == "guard"
    assert [option["kind"] for option in advisory.ranked_options(options, {"maxResults": 2})] == ["loot", "talk"]
    assert advisory.clamp_score("x") == 0 and advisory.clamp_score(-5) == 0
    assert advisory.int_or_default(True, 3) == 3 and advisory.int_or_default("7") == 7
    assert advisory.result_limit({"maxResults": 1000}) == 100 and advisory.result_limit({"maxResults": 0}) == 1
