"""Launch profiles and the orchestrator that starts, tracks and stops owned FOnline processes."""

from __future__ import annotations

import json
import os
import re
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path, PurePosixPath, PureWindowsPath
from typing import Any

import foconfig
from ai_control_mcp import process_owner
from ai_control_mcp.registry import InvalidArguments
from ai_control_mcp.session import Endpoint, Session, clamp


PROFILE_SCHEMA_VERSION = 1
ENDPOINT_MODES = ("none", "client", "embedded-clients")
SETTING_NAME_PATTERN = re.compile(r"^[A-Za-z][A-Za-z0-9_]*(\.[A-Za-z][A-Za-z0-9_]*)+$")
ENV_NAME_PATTERN = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
ROLE_NAME_PATTERN = re.compile(r"^[a-z][a-z0-9_]*$")
NON_INTERACTIVE_SETTING = "Common.NonInteractive"
EMBEDDED_CLIENTS_SETTING = "Server.AutoStartClientOnServer"
DEFAULT_SEARCH_DIRS = ("Binaries", "Build", "Workspace")
EXIT_STOP_TIMEOUT_MS = 3000
LOG_TAIL_MAX_LINES = 2000
LOG_TAIL_MAX_BYTES = 1024 * 1024

# Standard generated application targets named <FO_DEV_NAME>_<suffix>
STANDARD_ROLES: dict[str, dict[str, Any]] = {
    "server": {"target": "Server", "endpoints": "embedded-clients"},
    "server_headless": {"target": "ServerHeadless", "endpoints": "embedded-clients"},
    "client": {"target": "Client", "endpoints": "client"},
    "client_headless": {"target": "ClientHeadless", "endpoints": "client"},
    "mapper": {"target": "Mapper", "endpoints": "none"},
}


class LaunchProfileError(ValueError):
    pass


@dataclass(frozen=True)
class RoleSpec:
    name: str
    binary: str
    endpoints: str
    default_client_count: int
    settings: dict[str, Any]


@dataclass(frozen=True)
class BridgeSettings:
    enabled: str
    host: str
    port: str
    token: str | None
    port_stride: str | None


@dataclass(frozen=True)
class LaunchProfile:
    config: str | None
    roles: dict[str, RoleSpec]
    default_role: str
    default_sub_configs: tuple[str, ...]
    search_dirs: tuple[str, ...]
    binary_env_template: str | None
    bridge: BridgeSettings | None
    non_interactive: bool


def parse_launch_profile(raw: Any) -> LaunchProfile:
    if not isinstance(raw, dict):
        raise LaunchProfileError("launch profile must be a JSON object")

    known = {
        "schemaVersion", "config", "devName", "roles", "defaultRole", "defaultSubConfigs",
        "binarySearchDirs", "binaryEnvTemplate", "bridge", "nonInteractive",
    }
    _reject_unknown(raw, known, "launch profile")

    if raw.get("schemaVersion") != PROFILE_SCHEMA_VERSION:
        raise LaunchProfileError(f"launch profile schemaVersion must be {PROFILE_SCHEMA_VERSION}")

    config = _optional_string(raw, "config", "launch profile")
    dev_name = _optional_string(raw, "devName", "launch profile")
    roles_raw = raw.get("roles")

    if roles_raw is None:
        if dev_name is None:
            raise LaunchProfileError("launch profile needs devName or an explicit roles object")
        roles_raw = {name: dict(spec) for name, spec in STANDARD_ROLES.items()}

    if not isinstance(roles_raw, dict) or not roles_raw:
        raise LaunchProfileError("launch profile roles must be a non-empty object")

    roles: dict[str, RoleSpec] = {}

    for name, spec in roles_raw.items():
        roles[name] = _parse_role(name, spec, dev_name)

    default_role = raw.get("defaultRole", next(iter(roles)))

    if default_role not in roles:
        raise LaunchProfileError(f"launch profile defaultRole {default_role!r} is not a declared role")

    sub_configs = _string_list(raw.get("defaultSubConfigs", []), "launch profile defaultSubConfigs")
    search_dirs = _string_list(raw.get("binarySearchDirs", list(DEFAULT_SEARCH_DIRS)), "launch profile binarySearchDirs")

    for directory in search_dirs:
        _require_relative(directory, "launch profile binarySearchDirs entry")

    template = _optional_string(raw, "binaryEnvTemplate", "launch profile")

    if template is not None and not ENV_NAME_PATTERN.fullmatch(template.replace("{ROLE}", "ROLE")):
        raise LaunchProfileError("launch profile binaryEnvTemplate must be an environment variable name with an optional {ROLE}")

    non_interactive = raw.get("nonInteractive", True)

    if not isinstance(non_interactive, bool):
        raise LaunchProfileError("launch profile nonInteractive must be a boolean")

    return LaunchProfile(
        config=config,
        roles=roles,
        default_role=default_role,
        default_sub_configs=tuple(sub_configs),
        search_dirs=tuple(search_dirs),
        binary_env_template=template,
        bridge=_parse_bridge(raw.get("bridge")),
        non_interactive=non_interactive,
    )


def load_launch_profile(path: Path) -> LaunchProfile:
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exception:
        raise LaunchProfileError(f"cannot read launch profile {path}: {exception}") from exception

    return parse_launch_profile(raw)


def _parse_role(name: Any, spec: Any, dev_name: str | None) -> RoleSpec:
    if not isinstance(name, str) or not ROLE_NAME_PATTERN.fullmatch(name):
        raise LaunchProfileError(f"launch role name must match {ROLE_NAME_PATTERN.pattern}: {name!r}")
    if not isinstance(spec, dict):
        raise LaunchProfileError(f"launch role {name} must be an object")

    label = f"launch role {name}"
    _reject_unknown(spec, {"binary", "target", "endpoints", "defaultClientCount", "settings"}, label)
    binary = _optional_string(spec, "binary", label)
    target = _optional_string(spec, "target", label)

    if (binary is None) == (target is None):
        raise LaunchProfileError(f"{label} needs exactly one of binary or target")

    if target is not None:
        if dev_name is None:
            raise LaunchProfileError(f"{label} uses target and therefore needs the profile devName")
        binary = f"{dev_name}_{target}"

    assert binary is not None

    if PurePosixPath(binary).name != binary or PureWindowsPath(binary).name != binary:
        raise LaunchProfileError(f"{label} binary must be a file name without directories")

    endpoints = spec.get("endpoints", "none")

    if endpoints not in ENDPOINT_MODES:
        raise LaunchProfileError(f"{label} endpoints must be one of {', '.join(ENDPOINT_MODES)}")

    client_count = spec.get("defaultClientCount", 0)

    if isinstance(client_count, bool) or not isinstance(client_count, int) or client_count < 0:
        raise LaunchProfileError(f"{label} defaultClientCount must be a non-negative integer")

    settings = spec.get("settings", {})

    if not isinstance(settings, dict):
        raise LaunchProfileError(f"{label} settings must be an object")

    for key, value in settings.items():
        _check_setting(key, value, label)

    return RoleSpec(name, binary, endpoints, client_count, dict(settings))


def _parse_bridge(raw: Any) -> BridgeSettings | None:
    if raw is None:
        return None
    if not isinstance(raw, dict):
        raise LaunchProfileError("launch profile bridge must be an object")

    label = "launch profile bridge"
    _reject_unknown(raw, {"enabledSetting", "hostSetting", "portSetting", "tokenSetting", "portStrideSetting"}, label)
    names: dict[str, str | None] = {}

    for key in ("enabledSetting", "hostSetting", "portSetting", "tokenSetting", "portStrideSetting"):
        value = _optional_string(raw, key, label)

        if value is not None and not SETTING_NAME_PATTERN.fullmatch(value):
            raise LaunchProfileError(f"{label} {key} must be a Group.Name setting: {value!r}")

        names[key] = value

    for key in ("enabledSetting", "hostSetting", "portSetting"):
        if names[key] is None:
            raise LaunchProfileError(f"{label} needs {key}")

    return BridgeSettings(
        enabled=str(names["enabledSetting"]),
        host=str(names["hostSetting"]),
        port=str(names["portSetting"]),
        token=names["tokenSetting"],
        port_stride=names["portStrideSetting"],
    )


def _reject_unknown(raw: dict[str, Any], known: set[str], label: str) -> None:
    unknown = sorted(set(raw) - known)

    if unknown:
        raise LaunchProfileError(f"{label} has unknown keys: {', '.join(unknown)}")


def _optional_string(raw: dict[str, Any], key: str, label: str) -> str | None:
    value = raw.get(key)

    if value is None:
        return None
    if not isinstance(value, str) or not value:
        raise LaunchProfileError(f"{label} {key} must be a non-empty string")

    return value


def _string_list(value: Any, label: str) -> list[str]:
    if not isinstance(value, list) or any(not isinstance(item, str) or not item for item in value):
        raise LaunchProfileError(f"{label} must be an array of non-empty strings")

    return list(value)


def _require_relative(value: str, label: str) -> None:
    path = PurePosixPath(value.replace("\\", "/"))

    if path.is_absolute() or PureWindowsPath(value).is_absolute() or ".." in path.parts:
        raise LaunchProfileError(f"{label} must be a project-relative path without '..': {value!r}")


def _check_setting(key: Any, value: Any, label: str) -> None:
    if not isinstance(key, str) or not SETTING_NAME_PATTERN.fullmatch(key):
        raise LaunchProfileError(f"{label} setting name must be Group.Name: {key!r}")
    if isinstance(value, (dict, list)) or value is None:
        raise LaunchProfileError(f"{label} setting {key} must be a string, number or boolean")


def format_setting_value(value: Any) -> str:
    if isinstance(value, bool):
        return "True" if value else "False"

    text = str(value)

    # An option-like value keeps its value slot; the engine trims the leading space
    return " " + text if text.startswith("-") else text


@dataclass
class ManagedProcess:
    process_id: int
    role: str
    binary: Path
    command: list[str]
    cwd: Path
    process: Any
    started_at: float
    endpoints: list[Endpoint]

    def public(self) -> dict[str, Any]:
        return_code = self.process.poll()
        return {
            "processId": self.process_id,
            "osPid": self.process.pid,
            "role": self.role,
            "running": return_code is None,
            "returnCode": return_code,
            "startedAt": self.started_at,
            "cwd": str(self.cwd),
            "command": list(self.command),
            "endpoints": [endpoint.public() for endpoint in self.endpoints],
        }


class Orchestrator:
    """Starts processes under independent owners and stops only the trees it started."""

    def __init__(
        self,
        project_root: Path,
        session: Session,
        profile: LaunchProfile | None,
        *,
        token_env: str,
        keep_processes_on_exit: bool = False,
    ) -> None:
        self.project_root = project_root.resolve()
        self.session = session
        self.profile = profile
        self.token_env = token_env
        self.keep_processes_on_exit = keep_processes_on_exit
        self.processes: dict[int, ManagedProcess] = {}
        self._next_process_id = 1

    def require_profile(self) -> LaunchProfile:
        if self.profile is None:
            raise InvalidArguments("launching needs a launch profile: pass --launch-profile or set one in the project module")

        return self.profile

    def launch(self, arguments: dict[str, Any]) -> dict[str, Any]:
        profile = self.require_profile()
        role_name = str(arguments.get("role", profile.default_role))
        role = profile.roles.get(role_name)

        if role is None:
            raise InvalidArguments(f"role must be one of: {', '.join(profile.roles)}")

        binary = self.resolve_binary(profile, role, arguments.get("binaryPath"))
        cwd = self.resolve_inside_root(arguments.get("workingDirectory"), "workingDirectory") if arguments.get("workingDirectory") else self.project_root
        settings, endpoint_plan = self.launch_settings(profile, role, arguments)
        command = [str(binary)]

        if profile.config is not None:
            command.extend(["-ApplyConfig", str(self.project_root / profile.config)])

        for sub_config in list(profile.default_sub_configs) + _argument_strings(arguments, "subConfigs"):
            command.extend(["-ApplySubConfig", sub_config])

        for key, value in settings.items():
            command.extend([f"--{key}", format_setting_value(value)])

        process = process_owner.spawn_managed_process(command, str(cwd), self.keep_processes_on_exit, dict(os.environ))

        try:
            process_id = self._next_process_id
            self._next_process_id += 1
            endpoints = [
                self.session.add_endpoint(
                    host, port, endpoint_id=f"p{process_id}:e{index}", process_id=process_id, index=index, role=role.name
                )
                for index, (host, port) in enumerate(endpoint_plan)
            ]
            managed = ManagedProcess(process_id, role.name, binary, command, cwd, process, time.time(), endpoints)
            self.processes[process_id] = managed
        except BaseException:
            # The tree already belongs to an owner; registration failure must not leave it running
            process.kill()
            process.wait(timeout=EXIT_STOP_TIMEOUT_MS / 1000.0)
            raise

        wait_result: dict[str, Any] | None = None

        if endpoints and bool(arguments.get("waitForBridge", True)):
            wait_result = self.wait_for_endpoints(
                managed, int(arguments.get("timeoutMs", 15000)), int(arguments.get("probeIntervalMs", 250))
            )

        selected: dict[str, Any] | None = None

        if endpoints and bool(arguments.get("selectFirstEndpoint", True)):
            selected = self.session.select(endpoints[0]).public()

        return {**managed.public(), "binary": binary_build_stamp(binary), "selectedEndpoint": selected, "wait": wait_result}

    def launch_settings(
        self, profile: LaunchProfile, role: RoleSpec, arguments: dict[str, Any]
    ) -> tuple[dict[str, Any], list[tuple[str, int]]]:
        overrides = arguments.get("settings") or {}

        if not isinstance(overrides, dict):
            raise InvalidArguments("settings must be an object")

        for key, value in overrides.items():
            try:
                _check_setting(key, value, "launch")
            except LaunchProfileError as exception:
                raise InvalidArguments(str(exception)) from exception

        if profile.bridge is not None and profile.bridge.token is not None and profile.bridge.token in overrides:
            raise InvalidArguments(f"settings must not set {profile.bridge.token}; the adapter passes its token through the environment")

        settings: dict[str, Any] = dict(role.settings)
        client_count = arguments.get("clientCount", role.default_client_count)

        if isinstance(client_count, bool) or not isinstance(client_count, int) or client_count < 0:
            raise InvalidArguments("clientCount must be a non-negative integer")

        if role.endpoints == "embedded-clients":
            settings[EMBEDDED_CLIENTS_SETTING] = client_count

        endpoint_count = {"none": 0, "client": 1, "embedded-clients": client_count}[role.endpoints]
        bridge_enabled = profile.bridge is not None and endpoint_count > 0 and bool(arguments.get("bridge", True))
        plan: list[tuple[str, int]] = []

        if bridge_enabled:
            assert profile.bridge is not None
            host = str(arguments.get("bridgeHost", self.session.default_endpoint.host))
            port = arguments.get("bridgePort", self.session.default_endpoint.port)
            stride = arguments.get("bridgePortStride", 1)

            if isinstance(port, bool) or not isinstance(port, int) or not 1 <= port <= 65535:
                raise InvalidArguments("bridgePort must be an integer between 1 and 65535")
            if isinstance(stride, bool) or not isinstance(stride, int) or stride < 1:
                raise InvalidArguments("bridgePortStride must be a positive integer")
            if endpoint_count > 1 and profile.bridge.port_stride is None:
                raise InvalidArguments("more than one embedded client needs bridge.portStrideSetting in the launch profile")
            if port + (endpoint_count - 1) * stride > 65535:
                raise InvalidArguments("bridge ports for every embedded client must stay below 65536")

            self.session.check_host(host)
            settings[profile.bridge.enabled] = True
            settings[profile.bridge.host] = host
            settings[profile.bridge.port] = port

            if profile.bridge.port_stride is not None:
                settings[profile.bridge.port_stride] = stride

            if self.session.token:
                if profile.bridge.token is None:
                    raise InvalidArguments("the adapter has a token but the launch profile names no bridge.tokenSetting")

                # The value is a target-time directive: the token itself never appears on a command line
                settings[profile.bridge.token] = "$TARGET_ENV{" + self.token_env + "}"

            plan = [(host, port + index * stride) for index in range(endpoint_count)]

            if profile.non_interactive:
                settings[NON_INTERACTIVE_SETTING] = True

        settings.update(overrides)
        return settings, plan

    def resolve_binary(self, profile: LaunchProfile, role: RoleSpec, requested: Any) -> Path:
        if requested:
            path = self.resolve_inside_root(requested, "binaryPath")

            if not path.is_file():
                raise InvalidArguments(f"binaryPath does not name a file: {path}")

            return path

        name = role.binary + (".exe" if os.name == "nt" else "")

        if profile.binary_env_template is not None:
            variable = profile.binary_env_template.replace("{ROLE}", role.name.upper())
            value = os.environ.get(variable)

            if value:
                path = Path(value)
                path = path if path.is_absolute() else self.project_root / path

                if not path.is_file():
                    raise InvalidArguments(f"{variable} does not name a file: {path}")

                return path.resolve()

        found: list[Path] = []

        for directory in profile.search_dirs:
            root = self.project_root / directory

            if root.is_dir():
                found.extend(candidate for candidate in root.rglob(name) if candidate.is_file())

        if not found:
            raise InvalidArguments(f"no {name} under {', '.join(profile.search_dirs)}; build the target or pass binaryPath")

        # The newest build wins: a stale copy elsewhere must never silently run instead of a fresh one
        return max(found, key=lambda candidate: (candidate.stat().st_mtime, str(candidate))).resolve()

    def resolve_inside_root(self, value: Any, label: str) -> Path:
        if not isinstance(value, str) or not value:
            raise InvalidArguments(f"{label} must be a non-empty string")

        path = Path(value)
        path = (path if path.is_absolute() else self.project_root / path).resolve()

        if not path.is_relative_to(self.project_root):
            raise InvalidArguments(f"{label} must stay inside the project root {self.project_root}")

        return path

    def wait_for_endpoints(self, managed: ManagedProcess, timeout_ms: int, probe_interval_ms: int) -> dict[str, Any]:
        deadline = time.monotonic() + clamp(timeout_ms, 0, 600000) / 1000.0
        interval = max(clamp(probe_interval_ms, 0, 5000), 50) / 1000.0

        while True:
            states = [{**endpoint.public(), "ready": self.session.probe(endpoint)} for endpoint in managed.endpoints]

            if all(state["ready"] for state in states):
                return {"ready": True, "timedOut": False, "endpoints": states}

            return_code = managed.process.poll()

            if return_code is not None:
                return {"ready": False, "timedOut": False, "processExited": True, "returnCode": return_code, "endpoints": states}

            if time.monotonic() >= deadline:
                return {"ready": False, "timedOut": True, "endpoints": states}

            time.sleep(interval)

    def list_processes(self) -> list[dict[str, Any]]:
        return [managed.public() for managed in self.processes.values()]

    def process(self, process_id: int) -> ManagedProcess:
        managed = self.processes.get(process_id)

        if managed is None:
            raise InvalidArguments(f"unknown processId: {process_id}")

        return managed

    def endpoint_process_exit(self, endpoint: Endpoint) -> dict[str, Any] | None:
        if endpoint.process_id is None or endpoint.process_id not in self.processes:
            return None

        return_code = self.processes[endpoint.process_id].process.poll()

        if return_code is None:
            return None

        return {"processExited": True, "processId": endpoint.process_id, "returnCode": return_code}

    def stop(self, process_id: int | None, all_processes: bool, timeout_ms: int) -> dict[str, Any]:
        if all_processes:
            targets = list(self.processes.values())
        elif process_id is not None:
            targets = [self.process(process_id)]
        else:
            raise InvalidArguments("pass processId or all=true")

        timeout = clamp(timeout_ms, 0, 600000) / 1000.0
        stopped: list[dict[str, Any]] = []

        for managed in targets:
            if managed.process.poll() is None:
                managed.process.terminate()

                try:
                    managed.process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    managed.process.kill()
                    managed.process.wait(timeout=timeout if timeout > 0 else 5.0)

            stopped.append({"processId": managed.process_id, "osPid": managed.process.pid, "returnCode": managed.process.poll()})
            self.processes.pop(managed.process_id, None)
            self.session.remove_process_endpoints(managed.process_id)

        return {"stopped": stopped}

    def stop_all_for_exit(self) -> dict[str, Any] | None:
        if self.keep_processes_on_exit or not self.processes:
            return None

        return self.stop(None, True, EXIT_STOP_TIMEOUT_MS)

    def launch_options(self) -> dict[str, Any]:
        profile = self.profile

        if profile is None:
            return {"configured": False, "roles": []}

        result: dict[str, Any] = {
            "configured": True,
            "projectRoot": str(self.project_root),
            "config": profile.config,
            "defaultRole": profile.default_role,
            "defaultSubConfigs": list(profile.default_sub_configs),
            "bridge": profile.bridge is not None,
            "keepProcessesOnExit": self.keep_processes_on_exit,
            "roles": [
                {"role": role.name, "binary": role.binary, "endpoints": role.endpoints, "defaultClientCount": role.default_client_count}
                for role in profile.roles.values()
            ],
            "subConfigs": [],
        }

        if profile.config is not None:
            config_path = self.project_root / profile.config

            if config_path.is_file():
                parser = foconfig.ConfigParser()
                parser.load_from_file(config_path)
                result["subConfigs"] = [
                    {"name": section.content.get("Name", ""), "parent": section.content.get("Parent", "")}
                    for section in parser.get_sections("SubConfig")
                ]
            else:
                result["configMissing"] = True

        return result

    def log_tail(self, arguments: dict[str, Any]) -> dict[str, Any]:
        managed: ManagedProcess | None = None
        cwd = self.project_root
        log_name = arguments.get("logName")

        if "processId" in arguments:
            managed = self.process(int(arguments["processId"]))
            cwd = managed.cwd
            log_name = log_name or managed.binary.stem + ".log"

        if not isinstance(log_name, str) or not log_name:
            raise InvalidArguments("pass processId or logName")
        if PureWindowsPath(log_name).name != log_name or PurePosixPath(log_name).name != log_name:
            raise InvalidArguments("logName must be a file name, not a path")

        path = (cwd / log_name).resolve()

        if not path.is_relative_to(self.project_root) and not path.is_relative_to(cwd.resolve()):
            raise InvalidArguments("resolved log path is outside the project root and the process working directory")

        lines = clamp(int(arguments.get("lines", 200)), 1, LOG_TAIL_MAX_LINES)
        max_bytes = clamp(int(arguments.get("maxBytes", 262144)), 1024, LOG_TAIL_MAX_BYTES)
        text, truncated = read_tail(path, max_bytes, lines)
        result: dict[str, Any] = {"path": str(path), "exists": path.is_file(), "text": text, "truncatedBytes": truncated}

        if managed is not None:
            result["process"] = managed.public()

        return result


def read_tail(path: Path, max_bytes: int, lines: int) -> tuple[str, bool]:
    if not path.is_file():
        return "", False

    with path.open("rb") as stream:
        stream.seek(0, os.SEEK_END)
        size = stream.tell()
        start = max(size - max_bytes, 0)
        stream.seek(start)
        data = stream.read()

    text = data.decode("utf-8", errors="replace")

    if start > 0:
        # Drop the partial first line cut by the byte window
        text = text.split("\n", 1)[1] if "\n" in text else ""

    return "\n".join(text.splitlines()[-lines:]), start > 0


def binary_build_stamp(path: Path) -> dict[str, Any]:
    stamp: dict[str, Any] = {"path": str(path), "modifiedAt": int(path.stat().st_mtime)}
    marker = path.with_name(f"{path.stem}.build-hash")

    # The build writes <target>.build-hash beside the executable; it identifies the exact build that ran
    try:
        stamp["buildHash"] = marker.read_text(encoding="utf-8", errors="replace").strip()[:64]
    except OSError:
        stamp["buildHash"] = None

    return stamp


def _argument_strings(arguments: dict[str, Any], key: str) -> list[str]:
    value = arguments.get(key, [])

    if not isinstance(value, list) or any(not isinstance(item, str) or not item for item in value):
        raise InvalidArguments(f"{key} must be an array of non-empty strings")

    return list(value)
