"""State and helpers handed to every tool handler."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

from ai_control_mcp.launch import Orchestrator
from ai_control_mcp.registry import ToolRegistry
from ai_control_mcp.session import Endpoint, Session, field_readiness


ENDPOINT_TARGET_ARGUMENTS = ("endpointId", "processId", "endpointIndex")
ACT_OPTION_ARGUMENTS = (
    "waitForCompletion", "syncAfterCompletion", "timeoutMs", "pollIntervalMs", "limit",
    "includeEvents", "maxReturnedEvents", "includeObservation", "includeStatus", "freshTimeoutMs",
)


@dataclass
class ToolContext:
    registry: ToolRegistry
    session: Session
    orchestrator: Orchestrator
    project_root: Path

    def endpoint(self, arguments: dict[str, Any]) -> Endpoint:
        """The endpoint named by endpointId or processId/endpointIndex, else the selected one."""
        return self.session.resolve(arguments)

    def request(self, arguments: dict[str, Any], method: str, params: dict[str, Any] | None = None) -> Any:
        return self.session.request(self.endpoint(arguments), method, params)

    def observe(self, arguments: dict[str, Any]) -> dict[str, Any]:
        return self.session.observe(self.endpoint(arguments))

    def act(self, arguments: dict[str, Any], command: dict[str, Any]) -> dict[str, Any]:
        """Send one project command; wait for its fresh completion when the arguments ask for it."""
        options = {key: arguments[key] for key in ACT_OPTION_ARGUMENTS if key in arguments}
        return self.session.act_and_wait(self.endpoint(arguments), command, options)

    def readiness_missing(self, observation: dict[str, Any], arguments: dict[str, Any]) -> list[str]:
        check = self.registry.readiness_check or field_readiness(self.registry.readiness_fields)
        return list(check(observation, arguments))


def without_target_arguments(arguments: dict[str, Any]) -> dict[str, Any]:
    return {key: value for key, value in arguments.items() if key not in ENDPOINT_TARGET_ARGUMENTS}
