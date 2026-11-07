"""Input-schema fragments shared by core and project tools."""

from __future__ import annotations

from typing import Any

from ai_control_mcp.session import MAX_EVENT_PAGE, MAX_POLL_INTERVAL_MS, MAX_WAIT_MS


def endpoint_target_properties() -> dict[str, Any]:
    return {
        "endpointId": {"type": "string", "description": "Registered endpoint id from launch, connect or endpoints."},
        "processId": {"type": "integer", "minimum": 1, "description": "Managed process id returned by launch."},
        "endpointIndex": {"type": "integer", "minimum": 0, "description": "Endpoint index inside a managed process; default 0."},
    }


def act_option_properties() -> dict[str, Any]:
    return {
        "waitForCompletion": {"type": "boolean", "description": "Wait for this command's command_completed event."},
        "syncAfterCompletion": {"type": "boolean", "description": "Wait, then return a fresh observation; implies waitForCompletion."},
        "timeoutMs": {"type": "integer", "minimum": 0, "maximum": MAX_WAIT_MS, "description": "Completion wait limit; default 3000."},
        "pollIntervalMs": {"type": "integer", "minimum": 0, "maximum": MAX_POLL_INTERVAL_MS},
        "limit": {"type": "integer", "minimum": 1, "maximum": MAX_EVENT_PAGE, "description": "Events read per poll."},
        "includeEvents": {"type": "boolean", "description": "Return the events consumed while waiting; default true."},
        "maxReturnedEvents": {"type": "integer", "minimum": 0, "maximum": 1000},
        "includeObservation": {"type": "boolean", "description": "Sync: include the observation; default true."},
        "includeStatus": {"type": "boolean", "description": "Sync: include bridge status; default false."},
        "freshTimeoutMs": {"type": "integer", "minimum": 0, "maximum": MAX_WAIT_MS, "description": "Sync: wait limit for a newer observation; default 3000."},
    }


def readiness_properties() -> dict[str, Any]:
    return {
        "timeoutMs": {"type": "integer", "minimum": 0, "maximum": MAX_WAIT_MS, "description": "Readiness wait limit; default 60000."},
        "pollIntervalMs": {"type": "integer", "minimum": 0, "maximum": MAX_POLL_INTERVAL_MS},
        "requireFields": {
            "type": "array",
            "items": {"type": "string", "minLength": 1},
            "description": "Observation fields that must be truthy; replaces the project's default gate fields.",
        },
        "includeObservation": {"type": "boolean", "description": "Include the last observation; default true."},
    }
