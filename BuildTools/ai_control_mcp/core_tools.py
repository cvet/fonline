"""Protocol-generic tools: the six AiControl methods plus endpoint, readiness and process control."""

from __future__ import annotations

from typing import Any

from ai_control_mcp.context import ToolContext
from ai_control_mcp.registry import InvalidArguments, ToolRegistry, object_schema
from ai_control_mcp.schemas import act_option_properties, endpoint_target_properties, readiness_properties
from ai_control_mcp.session import MAX_EVENT_PAGE, MAX_POLL_INTERVAL_MS, MAX_WAIT_MS


READ_ONLY = {"readOnlyHint": True}


def register_core_tools(registry: ToolRegistry) -> None:
    target = endpoint_target_properties()

    def add(base_name: str, handler: Any, description: str, properties: dict[str, Any], required: list[str] | None = None,
            annotations: dict[str, Any] | None = None) -> None:
        registry.add_tool(
            registry.core_name(base_name),
            handler,
            description=description,
            input_schema=object_schema(properties, required),
            annotations=annotations,
        )

    add("status", tool_status, "Read AiControl bridge status: queue and event occupancy, limits, observation sequence, last error.",
        target, annotations=READ_ONLY)
    add("ping", tool_ping, "Check AiControl bridge liveness; this is transport health, not game readiness.", target, annotations=READ_ONLY)
    add("observe", tool_observe, "Read the latest project observation and its observationSeq; fresh=true waits for a newer snapshot.",
        {**target, "fresh": {"type": "boolean"}, "freshTimeoutMs": act_option_properties()["freshTimeoutMs"]}, annotations=READ_ONLY)
    add("events", tool_events, "Read retained events after an explicit afterSeq without moving the endpoint cursor.",
        {**target, "afterSeq": {"type": "integer", "minimum": 0}, "limit": {"type": "integer", "minimum": 1, "maximum": MAX_EVENT_PAGE}},
        annotations=READ_ONLY)
    add("next_events", tool_next_events, "Read events after this endpoint's cursor and advance it; reset or seekLatest reposition it.",
        {**target, "limit": {"type": "integer", "minimum": 1, "maximum": MAX_EVENT_PAGE}, "reset": {"type": "boolean"},
         "seekLatest": {"type": "boolean"}})
    add("act", tool_act, "Queue one project command object with a non-empty type; optionally wait for its fresh completion.",
        {**target, "command": {"type": "object", "description": "Project command; see the project's observation for action names."},
         **act_option_properties()}, ["command"])
    add("wait_command", tool_wait_command, "Wait for command_completed of an accepted commandSeq recorded after afterSeq.",
        {**target, "commandSeq": {"type": "integer", "minimum": 1}, "afterSeq": {"type": "integer", "minimum": 0},
         "timeoutMs": {"type": "integer", "minimum": 0, "maximum": MAX_WAIT_MS},
         "pollIntervalMs": {"type": "integer", "minimum": 0, "maximum": MAX_POLL_INTERVAL_MS},
         "limit": {"type": "integer", "minimum": 1, "maximum": MAX_EVENT_PAGE}, "includeEvents": {"type": "boolean"},
         "maxReturnedEvents": {"type": "integer", "minimum": 0, "maximum": 1000}}, ["commandSeq"])
    add("connect", tool_connect, "Select an endpoint (registered id, managed process, or loopback host/port) and verify it with ping.",
        {**target, "host": {"type": "string", "minLength": 1}, "port": {"type": "integer", "minimum": 1, "maximum": 65535},
         "verify": {"type": "boolean", "description": "Ping the endpoint after selecting it; default true."}})
    add("endpoints", tool_endpoints, "List registered endpoints, the selection and cursors; tokens are never shown.",
        {"probe": {"type": "boolean"}}, annotations=READ_ONLY)
    add("wait_ready", tool_wait_ready, "Poll observe until the project's readiness gate passes, the process exits, or timeout.",
        {**target, **readiness_properties()}, annotations=READ_ONLY)
    add("wait_all_ready", tool_wait_all_ready, "Wait until every listed endpoint, or every endpoint of a process, is ready.",
        {"endpointIds": {"type": "array", "items": {"type": "string"}, "minItems": 1}, "processId": {"type": "integer", "minimum": 1},
         **readiness_properties()}, annotations=READ_ONLY)
    add("launch_options", tool_launch_options, "Describe the launch profile: roles, binaries, sub-configs and bridge wiring.", {},
        annotations=READ_ONLY)
    add("launch", tool_launch, "Start a role from the launch profile under an independent process owner and register its endpoints.",
        {"role": {"type": "string"}, "subConfigs": {"type": "array", "items": {"type": "string", "minLength": 1}},
         "settings": {"type": "object", "additionalProperties": {"type": ["string", "integer", "number", "boolean"]}},
         "clientCount": {"type": "integer", "minimum": 0}, "bridge": {"type": "boolean"}, "bridgeHost": {"type": "string"},
         "bridgePort": {"type": "integer", "minimum": 1, "maximum": 65535}, "bridgePortStride": {"type": "integer", "minimum": 1},
         "binaryPath": {"type": "string"}, "workingDirectory": {"type": "string"}, "waitForBridge": {"type": "boolean"},
         "timeoutMs": {"type": "integer", "minimum": 0, "maximum": MAX_WAIT_MS},
         "probeIntervalMs": {"type": "integer", "minimum": 0, "maximum": MAX_POLL_INTERVAL_MS},
         "selectFirstEndpoint": {"type": "boolean"}})
    add("processes", tool_processes, "List processes this adapter started and still owns.", {}, annotations=READ_ONLY)
    add("stop_process", tool_stop_process, "Stop one owned process tree, or all of them; never touches processes it did not start.",
        {"processId": {"type": "integer", "minimum": 1}, "all": {"type": "boolean"},
         "timeoutMs": {"type": "integer", "minimum": 0, "maximum": MAX_WAIT_MS}}, annotations={"destructiveHint": True})
    add("logs", tool_logs, "Read the tail of an owned process log (default <binary>.log) or a named log file in the project root.",
        {"processId": {"type": "integer", "minimum": 1}, "logName": {"type": "string", "minLength": 1},
         "lines": {"type": "integer", "minimum": 1, "maximum": 2000}, "maxBytes": {"type": "integer", "minimum": 1024, "maximum": 1048576}},
        annotations=READ_ONLY)


def tool_status(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return context.session.object_request(context.endpoint(arguments), "status")


def tool_ping(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return context.session.object_request(context.endpoint(arguments), "ping")


def tool_observe(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    endpoint = context.endpoint(arguments)

    if not arguments.get("fresh"):
        return context.session.observe(endpoint)

    observation, fresh = context.session.observe_fresh(endpoint, int(arguments.get("freshTimeoutMs", 3000)))
    return {**observation, "observationFresh": fresh}


def tool_events(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return context.session.events(context.endpoint(arguments), int(arguments.get("afterSeq", 0)), int(arguments.get("limit", 100)))


def tool_next_events(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return context.session.next_events(
        context.endpoint(arguments),
        limit=int(arguments.get("limit", 100)),
        reset=bool(arguments.get("reset", False)),
        seek_latest=bool(arguments.get("seekLatest", False)),
    )


def tool_act(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return context.act(arguments, dict(arguments["command"]))


def tool_wait_command(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return context.session.wait_command(
        context.endpoint(arguments),
        int(arguments["commandSeq"]),
        after_seq=arguments.get("afterSeq"),
        timeout_ms=int(arguments.get("timeoutMs", 3000)),
        poll_interval_ms=int(arguments.get("pollIntervalMs", 100)),
        limit=int(arguments.get("limit", 100)),
        include_events=bool(arguments.get("includeEvents", True)),
        max_returned_events=int(arguments.get("maxReturnedEvents", 100)),
    )


def tool_connect(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    has_direct = "host" in arguments or "port" in arguments

    if has_direct and any(key in arguments for key in ("endpointId", "processId", "endpointIndex")):
        raise InvalidArguments("pass either host/port or a registered endpoint, not both")

    if has_direct:
        if "port" not in arguments:
            raise InvalidArguments("port is required with host")

        endpoint = context.session.add_endpoint(str(arguments.get("host", context.session.default_endpoint.host)), arguments["port"])
    else:
        endpoint = context.endpoint(arguments)

    context.session.select(endpoint)
    result: dict[str, Any] = {"selectedEndpoint": endpoint.public()}

    if arguments.get("verify", True):
        result["ping"] = context.session.object_request(endpoint, "ping")

    return result


def tool_endpoints(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return context.session.summary(probe=bool(arguments.get("probe", False)))


def tool_wait_ready(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return context.session.wait_ready(
        context.endpoint(arguments), arguments, context.readiness_missing, context.orchestrator.endpoint_process_exit
    )


def tool_wait_all_ready(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    if "endpointIds" in arguments:
        endpoints = [context.session.endpoint(str(endpoint_id)) for endpoint_id in arguments["endpointIds"]]
    elif "processId" in arguments:
        endpoints = list(context.orchestrator.process(int(arguments["processId"])).endpoints)
    else:
        endpoints = context.session.endpoints()

    if not endpoints:
        raise InvalidArguments("no endpoints to wait for")

    return context.session.wait_all_ready(endpoints, arguments, context.readiness_missing, context.orchestrator.endpoint_process_exit)


def tool_launch_options(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return context.orchestrator.launch_options()


def tool_launch(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return context.orchestrator.launch(arguments)


def tool_processes(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return {"processes": context.orchestrator.list_processes()}


def tool_stop_process(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    process_id = arguments.get("processId")
    return context.orchestrator.stop(
        None if process_id is None else int(process_id), bool(arguments.get("all", False)), int(arguments.get("timeoutMs", 5000))
    )


def tool_logs(context: ToolContext, arguments: dict[str, Any]) -> dict[str, Any]:
    return context.orchestrator.log_tail(arguments)
