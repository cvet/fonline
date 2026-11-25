"""Endpoint registry, event cursors and command lifecycle on top of the reference AiControl client."""

from __future__ import annotations

import time
from dataclasses import dataclass
from typing import Any, Callable

from ai_control_client import (
    AiControlClient,
    AiControlError,
    AuthorizationError,
    ProtocolError,
    RemoteError,
    is_loopback_host,
)
from ai_control_mcp.registry import InvalidArguments


MAX_EVENT_SEQ = 2**63 - 1
MAX_WAIT_MS = 600000
MAX_POLL_INTERVAL_MS = 5000
MAX_EVENT_PAGE = 500
PROBE_TIMEOUT_SECONDS = 1.0
FRESH_OBSERVATION_TIMEOUT_MS = 3000
FRESH_OBSERVATION_POLL_MS = 100

ClientFactory = Callable[..., AiControlClient]
ReadinessCheck = Callable[[dict[str, Any], dict[str, Any]], list[str]]
ProcessExitCheck = Callable[["Endpoint"], "dict[str, Any] | None"]


@dataclass(frozen=True)
class Endpoint:
    endpoint_id: str
    host: str
    port: int
    process_id: int | None = None
    index: int = 0
    role: str = ""

    def public(self) -> dict[str, Any]:
        # The token is never part of an endpoint view: tool results end up in agent transcripts
        return {
            "endpointId": self.endpoint_id,
            "host": self.host,
            "port": self.port,
            "processId": self.process_id,
            "endpointIndex": self.index,
            "role": self.role,
        }


class Session:
    """Owns the adapter's endpoints and their event cursors."""

    def __init__(
        self,
        *,
        host: str,
        port: int,
        token: str,
        timeout: float,
        allow_remote: bool,
        client_factory: ClientFactory = AiControlClient,
    ) -> None:
        self.token = token
        self.timeout = timeout
        self.allow_remote = allow_remote
        self._client_factory = client_factory
        self._endpoints: dict[str, Endpoint] = {}
        self._cursors: dict[str, int] = {}
        self.selected_id: str | None = None
        self.default_endpoint = self.add_endpoint(host, port, endpoint_id="default")
        self.selected_id = self.default_endpoint.endpoint_id

    # Endpoint registry

    def check_host(self, host: str) -> None:
        if is_loopback_host(host):
            return
        if not self.allow_remote:
            raise InvalidArguments(f"non-loopback endpoint {host} requires the adapter's --allow-remote option")
        if not self.token:
            raise InvalidArguments(f"non-loopback endpoint {host} requires a non-empty token")

    def add_endpoint(
        self,
        host: str,
        port: int,
        *,
        endpoint_id: str | None = None,
        process_id: int | None = None,
        index: int = 0,
        role: str = "",
    ) -> Endpoint:
        if not isinstance(host, str) or not host:
            raise InvalidArguments("endpoint host must be a non-empty string")
        if isinstance(port, bool) or not isinstance(port, int) or not 1 <= port <= 65535:
            raise InvalidArguments("endpoint port must be an integer between 1 and 65535")

        self.check_host(host)
        identifier = endpoint_id or f"{host}:{port}"
        endpoint = Endpoint(identifier, host, port, process_id, index, role)
        previous = self._endpoints.get(identifier)

        if previous is not None and previous != endpoint:
            self._cursors[identifier] = 0

        self._endpoints[identifier] = endpoint
        self._cursors.setdefault(identifier, 0)
        return endpoint

    def remove_process_endpoints(self, process_id: int) -> None:
        for endpoint_id in [key for key, value in self._endpoints.items() if value.process_id == process_id]:
            self._endpoints.pop(endpoint_id, None)
            self._cursors.pop(endpoint_id, None)

            if self.selected_id == endpoint_id:
                self.selected_id = self.default_endpoint.endpoint_id

    def endpoints(self) -> list[Endpoint]:
        return list(self._endpoints.values())

    def endpoint(self, endpoint_id: str) -> Endpoint:
        endpoint = self._endpoints.get(endpoint_id)

        if endpoint is None:
            raise InvalidArguments(f"unknown endpointId: {endpoint_id}")

        return endpoint

    def process_endpoint(self, process_id: int, index: int) -> Endpoint:
        candidates = [endpoint for endpoint in self._endpoints.values() if endpoint.process_id == process_id]

        if not candidates:
            raise InvalidArguments(f"processId {process_id} has no registered endpoints")

        for endpoint in candidates:
            if endpoint.index == index:
                return endpoint

        raise InvalidArguments(f"endpointIndex {index} is out of range for processId {process_id}")

    def resolve(self, arguments: dict[str, Any]) -> Endpoint:
        if "endpointId" in arguments:
            return self.endpoint(str(arguments["endpointId"]))

        if "processId" in arguments:
            return self.process_endpoint(int(arguments["processId"]), int(arguments.get("endpointIndex", 0)))

        if "endpointIndex" in arguments:
            raise InvalidArguments("endpointIndex requires processId")

        assert self.selected_id is not None
        return self.endpoint(self.selected_id)

    def select(self, endpoint: Endpoint) -> Endpoint:
        self.selected_id = endpoint.endpoint_id
        return endpoint

    def cursor(self, endpoint: Endpoint) -> int:
        return self._cursors.get(endpoint.endpoint_id, 0)

    def set_cursor(self, endpoint: Endpoint, value: int) -> None:
        self._cursors[endpoint.endpoint_id] = max(int(value), 0)

    def summary(self, probe: bool = False) -> dict[str, Any]:
        entries: list[dict[str, Any]] = []

        for endpoint in self._endpoints.values():
            entry = endpoint.public()
            entry["selected"] = endpoint.endpoint_id == self.selected_id
            entry["cursor"] = self.cursor(endpoint)

            if probe:
                entry["ready"] = self.probe(endpoint)

            entries.append(entry)

        return {"selectedEndpointId": self.selected_id, "tokenConfigured": bool(self.token), "endpoints": entries}

    # Transport

    def request(self, endpoint: Endpoint, method: str, params: dict[str, Any] | None = None) -> Any:
        # One authenticated connection per request: a project listener may serve a single connection
        # at a time, and a held connection would lock out probes, diagnostics and other adapters
        client = self._client_factory(
            endpoint.host, endpoint.port, timeout=self.timeout, allow_remote=self.allow_remote, token=self.token
        )

        with client:
            return client.request(method, params or {})

    def probe(self, endpoint: Endpoint, timeout: float = PROBE_TIMEOUT_SECONDS) -> bool:
        client = self._client_factory(
            endpoint.host, endpoint.port, timeout=min(timeout, self.timeout), allow_remote=self.allow_remote, token=self.token
        )

        try:
            with client:
                client.ping()
            return True
        except (AiControlError, OSError):
            return False

    # Protocol methods

    def object_request(self, endpoint: Endpoint, method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
        result = self.request(endpoint, method, params)

        if not isinstance(result, dict):
            raise ProtocolError(f"{method} result must be an object")

        return result

    def observe(self, endpoint: Endpoint) -> dict[str, Any]:
        result = self.object_request(endpoint, "observe")
        sequence = result.get("observationSeq")

        if isinstance(sequence, bool) or not isinstance(sequence, int) or not isinstance(result.get("observation"), dict):
            raise ProtocolError("observe result must contain an integer observationSeq and an observation object")

        return result

    def events(self, endpoint: Endpoint, after_seq: int, limit: int) -> dict[str, Any]:
        result = self.object_request(endpoint, "events", {"afterSeq": after_seq, "limit": limit})
        latest = result.get("latestSeq")
        entries = result.get("events")

        if isinstance(latest, bool) or not isinstance(latest, int) or not isinstance(entries, list):
            raise ProtocolError("events result must contain an integer latestSeq and an events array")

        previous = after_seq

        for entry in entries:
            sequence = entry.get("seq") if isinstance(entry, dict) else None

            if isinstance(sequence, bool) or not isinstance(sequence, int) or not isinstance(entry.get("event"), dict):
                raise ProtocolError("every events entry must contain an integer seq and an event object")
            if sequence <= previous:
                raise ProtocolError("events entries must be newer than afterSeq and in ascending order")

            previous = sequence

        return result

    def latest_event_seq(self, endpoint: Endpoint) -> int:
        return int(self.events(endpoint, MAX_EVENT_SEQ, 1)["latestSeq"])

    def next_events(self, endpoint: Endpoint, *, limit: int, reset: bool = False, seek_latest: bool = False) -> dict[str, Any]:
        if reset:
            self.set_cursor(endpoint, 0)

        cursor = self.cursor(endpoint)

        if seek_latest:
            latest = self.latest_event_seq(endpoint)
            self.set_cursor(endpoint, latest)
            return {"latestSeq": latest, "events": [], "cursor": latest, "historyReset": latest < cursor}

        result = self.events(endpoint, cursor, limit)
        history_reset = self._advance_cursor(endpoint, cursor, result)
        return {**result, "cursor": self.cursor(endpoint), "historyReset": history_reset}

    def _advance_cursor(self, endpoint: Endpoint, scanned_from: int, result: dict[str, Any]) -> bool:
        latest = int(result["latestSeq"])
        entries = result["events"]

        if latest < scanned_from:
            # A restarted bridge numbers events from zero again; an old cursor would hide all of them
            self.set_cursor(endpoint, 0)
            return True

        position = int(entries[-1]["seq"]) if entries else latest
        self.set_cursor(endpoint, max(self.cursor(endpoint), position))
        return False

    def act(self, endpoint: Endpoint, command: dict[str, Any]) -> dict[str, Any]:
        command_type = command.get("type") if isinstance(command, dict) else None

        if not isinstance(command_type, str) or not command_type:
            raise InvalidArguments("command.type must be a non-empty string")

        result = self.object_request(endpoint, "act", command)
        sequence = result.get("commandSeq")

        if result.get("accepted") is not True or isinstance(sequence, bool) or not isinstance(sequence, int):
            raise ProtocolError("act result must contain accepted=true and an integer commandSeq")

        return result

    def wait_command(
        self,
        endpoint: Endpoint,
        command_seq: int,
        *,
        after_seq: int | None = None,
        timeout_ms: int = 3000,
        poll_interval_ms: int = 100,
        limit: int = 100,
        include_events: bool = True,
        max_returned_events: int = 100,
    ) -> dict[str, Any]:
        """Wait for the command_completed event of one accepted command.

        Only a completion recorded after ``after_seq`` counts, so a caller that captured the event
        position before sending ``act`` never accepts an older completion.
        """
        timeout_ms = clamp(timeout_ms, 0, MAX_WAIT_MS)
        poll_interval_ms = clamp(poll_interval_ms, 0, MAX_POLL_INTERVAL_MS)
        limit = clamp(limit, 1, MAX_EVENT_PAGE)
        max_returned_events = clamp(max_returned_events, 0, 1000)
        baseline = 0 if after_seq is None else max(after_seq, 0)
        scan_from = self.cursor(endpoint) if after_seq is None else min(self.cursor(endpoint), baseline)
        deadline = time.monotonic() + timeout_ms / 1000.0
        collected: list[dict[str, Any]] = []
        truncated = False
        latest = scan_from

        while True:
            result = self.events(endpoint, scan_from, limit)
            history_reset = self._advance_cursor(endpoint, scan_from, result)
            latest = int(result["latestSeq"])
            completion: dict[str, Any] | None = None

            if history_reset:
                return self._wait_result(command_seq, None, collected, truncated, endpoint, latest, history_reset=True)

            for entry in result["events"]:
                if include_events:
                    if len(collected) < max_returned_events:
                        collected.append(entry)
                    else:
                        truncated = True

                event = entry["event"]

                if (
                    completion is None
                    and entry["seq"] > baseline
                    and event.get("type") == "command_completed"
                    and event.get("commandSeq") == command_seq
                ):
                    completion = entry

            if completion is not None:
                return self._wait_result(command_seq, completion, collected, truncated, endpoint, latest)

            if result["events"]:
                scan_from = int(result["events"][-1]["seq"])

                if len(result["events"]) >= limit:
                    continue

            if time.monotonic() >= deadline:
                return self._wait_result(command_seq, None, collected, truncated, endpoint, latest)

            if poll_interval_ms > 0:
                time.sleep(poll_interval_ms / 1000.0)

    def _wait_result(
        self,
        command_seq: int,
        completion: dict[str, Any] | None,
        collected: list[dict[str, Any]],
        truncated: bool,
        endpoint: Endpoint,
        latest: int,
        history_reset: bool = False,
    ) -> dict[str, Any]:
        result: dict[str, Any] = {
            "completed": completion is not None,
            "timedOut": completion is None and not history_reset,
            "commandSeq": command_seq,
            "events": collected,
            "eventsTruncated": truncated,
            "cursor": self.cursor(endpoint),
            "latestSeq": latest,
        }

        if completion is not None:
            result["event"] = completion["event"]
            result["entry"] = completion

        if history_reset:
            result["historyReset"] = True

        return result

    def act_and_wait(self, endpoint: Endpoint, command: dict[str, Any], options: dict[str, Any]) -> dict[str, Any]:
        sync = bool(options.get("syncAfterCompletion", False))
        wait = bool(options.get("waitForCompletion", False)) or sync
        baseline = self.latest_event_seq(endpoint) if wait else 0
        result = dict(self.act(endpoint, command))

        if not wait:
            return result

        result["completion"] = self.wait_command(
            endpoint,
            int(result["commandSeq"]),
            after_seq=baseline,
            timeout_ms=int(options.get("timeoutMs", 3000)),
            poll_interval_ms=int(options.get("pollIntervalMs", 100)),
            limit=int(options.get("limit", 100)),
            include_events=bool(options.get("includeEvents", True)),
            max_returned_events=int(options.get("maxReturnedEvents", 100)),
        )

        if sync:
            result["sync"] = self.sync_snapshot(endpoint, options)

        return result

    def sync_snapshot(self, endpoint: Endpoint, options: dict[str, Any]) -> dict[str, Any]:
        snapshot: dict[str, Any] = {}

        if bool(options.get("includeObservation", True)):
            observation, fresh = self.observe_fresh(endpoint, int(options.get("freshTimeoutMs", FRESH_OBSERVATION_TIMEOUT_MS)))
            snapshot["observationSeq"] = observation["observationSeq"]
            snapshot["observation"] = observation["observation"]
            snapshot["observationFresh"] = fresh

        if bool(options.get("includeStatus", False)):
            snapshot["status"] = self.object_request(endpoint, "status")

        return snapshot

    def observe_fresh(self, endpoint: Endpoint, timeout_ms: int = FRESH_OBSERVATION_TIMEOUT_MS) -> tuple[dict[str, Any], bool]:
        """Return a snapshot published after the current one, or the current one when none arrives in time.

        A client publishes observations on its own schedule; the snapshot read right after a command
        can predate it. A strictly newer observationSeq was assembled after the first read.
        """
        current = self.observe(endpoint)
        baseline = int(current["observationSeq"])
        deadline = time.monotonic() + clamp(timeout_ms, 0, MAX_WAIT_MS) / 1000.0

        while time.monotonic() < deadline:
            time.sleep(FRESH_OBSERVATION_POLL_MS / 1000.0)
            newer = self.observe(endpoint)

            if int(newer["observationSeq"]) > baseline:
                return newer, True

        return current, False

    def wait_ready(
        self,
        endpoint: Endpoint,
        arguments: dict[str, Any],
        readiness: ReadinessCheck,
        process_exit: ProcessExitCheck,
    ) -> dict[str, Any]:
        timeout_ms = clamp(int(arguments.get("timeoutMs", 60000)), 0, MAX_WAIT_MS)
        poll_interval_ms = clamp(int(arguments.get("pollIntervalMs", 250)), 0, MAX_POLL_INTERVAL_MS)
        include_observation = bool(arguments.get("includeObservation", True))
        deadline = time.monotonic() + timeout_ms / 1000.0
        attempts = 0

        while True:
            attempts += 1
            state = self.poll_ready(endpoint, arguments, readiness, include_observation)
            state["attempts"] = attempts
            state["endpoint"] = endpoint.public()

            if state["ready"] or state.get("terminal"):
                state.pop("terminal", None)
                return {"timedOut": False, **state}

            exit_state = process_exit(endpoint)

            if exit_state is not None:
                return {"timedOut": False, **state, **exit_state}

            if time.monotonic() >= deadline:
                return {"timedOut": True, **state}

            if poll_interval_ms > 0:
                time.sleep(poll_interval_ms / 1000.0)

    def wait_all_ready(
        self,
        endpoints: list[Endpoint],
        arguments: dict[str, Any],
        readiness: ReadinessCheck,
        process_exit: ProcessExitCheck,
    ) -> dict[str, Any]:
        timeout_ms = clamp(int(arguments.get("timeoutMs", 60000)), 0, MAX_WAIT_MS)
        poll_interval_ms = clamp(int(arguments.get("pollIntervalMs", 250)), 0, MAX_POLL_INTERVAL_MS)
        include_observation = bool(arguments.get("includeObservation", True))
        deadline = time.monotonic() + timeout_ms / 1000.0
        states: dict[str, dict[str, Any]] = {
            endpoint.endpoint_id: {"endpoint": endpoint.public(), "ready": False, "attempts": 0} for endpoint in endpoints
        }

        while True:
            failed = False

            for endpoint in endpoints:
                state = states[endpoint.endpoint_id]

                if state["ready"]:
                    continue

                polled = self.poll_ready(endpoint, arguments, readiness, include_observation)
                terminal = bool(polled.pop("terminal", False))
                exit_state = None if polled["ready"] else process_exit(endpoint)
                state.update(polled)
                state["attempts"] += 1

                if exit_state is not None:
                    state.update(exit_state)

                failed = failed or terminal or exit_state is not None

            ready = all(state["ready"] for state in states.values())

            if ready or failed or time.monotonic() >= deadline:
                timed_out = not ready and not failed
                return {"ready": ready, "timedOut": timed_out, "endpoints": list(states.values())}

            if poll_interval_ms > 0:
                time.sleep(poll_interval_ms / 1000.0)

    def poll_ready(
        self, endpoint: Endpoint, arguments: dict[str, Any], readiness: ReadinessCheck, include_observation: bool
    ) -> dict[str, Any]:
        try:
            payload = self.observe(endpoint)
        except OSError as exception:
            # The bridge may simply not listen yet while its process starts
            return {"ready": False, "missing": ["bridge"], "error": str(exception)}
        except (AuthorizationError, ProtocolError, RemoteError) as exception:
            return {"ready": False, "missing": ["bridge"], "error": str(exception), "terminal": True}

        observation = payload["observation"]
        missing = list(readiness(observation, arguments))
        state: dict[str, Any] = {"ready": not missing, "missing": missing, "observationSeq": payload["observationSeq"]}

        if include_observation:
            state["observation"] = observation

        return state


def clamp(value: int, minimum: int, maximum: int) -> int:
    return max(minimum, min(int(value), maximum))


def field_readiness(default_fields: tuple[str, ...]) -> ReadinessCheck:
    """Readiness gate that requires every named observation field to be truthy."""

    def check(observation: dict[str, Any], arguments: dict[str, Any]) -> list[str]:
        fields = arguments.get("requireFields")
        names = list(default_fields) if fields is None else [str(name) for name in fields]
        return [name for name in names if not observation.get(name)]

    return check
