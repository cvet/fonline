#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Any, Mapping, Sequence


SCHEMA_VERSION = 1
REPORT_SCHEMA_VERSION = 1
PLACEHOLDER_RE = re.compile(r"\{([A-Za-z_][A-Za-z0-9_]*)\}")
ROOT_FIELDS = {
    "schema_version",
    "name",
    "default_timeout_seconds",
    "forbidden_markers",
    "scenarios",
}
SCENARIO_FIELDS = {"id", "timeout_seconds", "forbidden_markers", "processes"}
PROCESS_FIELDS = {
    "id",
    "command",
    "working_directory",
    "environment",
    "ready_marker",
    "ready_timeout_seconds",
    "required_markers",
    "forbidden_markers",
    "expected_exit_code",
}


class ManifestError(ValueError):
    pass


def _unknown_fields(value: Mapping[str, Any], allowed: set[str], label: str) -> None:
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise ManifestError(f"{label} has unknown field(s): {', '.join(unknown)}")


def _required_string(value: Mapping[str, Any], field: str, label: str) -> str:
    result = value.get(field)
    if not isinstance(result, str) or not result:
        raise ManifestError(f"{label}.{field} must be a non-empty string")
    return result


def _string_list(value: Any, label: str) -> list[str]:
    if value is None:
        return []
    if not isinstance(value, list) or any(not isinstance(item, str) or not item for item in value):
        raise ManifestError(f"{label} must be an array of non-empty strings")
    if len(value) != len(set(value)):
        raise ManifestError(f"{label} contains duplicate markers")
    return value


def _positive_seconds(value: Any, label: str, default: float | None = None) -> float:
    if value is None and default is not None:
        return default
    if isinstance(value, bool) or not isinstance(value, (int, float)) or value <= 0:
        raise ManifestError(f"{label} must be a positive number")
    return float(value)


def load_manifest(path: Path) -> dict[str, Any]:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ManifestError(f"unable to read manifest {path}: {error}") from error
    if not isinstance(manifest, dict):
        raise ManifestError("manifest root must be an object")

    _unknown_fields(manifest, ROOT_FIELDS, "manifest")
    if manifest.get("schema_version") != SCHEMA_VERSION:
        raise ManifestError(f"manifest.schema_version must be {SCHEMA_VERSION}")
    _required_string(manifest, "name", "manifest")
    default_timeout = _positive_seconds(manifest.get("default_timeout_seconds"), "manifest.default_timeout_seconds")
    _string_list(manifest.get("forbidden_markers"), "manifest.forbidden_markers")

    scenarios = manifest.get("scenarios")
    if not isinstance(scenarios, list) or not scenarios:
        raise ManifestError("manifest.scenarios must be a non-empty array")
    scenario_ids: set[str] = set()
    for scenario_index, scenario in enumerate(scenarios):
        scenario_label = f"manifest.scenarios[{scenario_index}]"
        if not isinstance(scenario, dict):
            raise ManifestError(f"{scenario_label} must be an object")
        _unknown_fields(scenario, SCENARIO_FIELDS, scenario_label)
        scenario_id = _required_string(scenario, "id", scenario_label)
        if scenario_id in scenario_ids:
            raise ManifestError(f"duplicate scenario id: {scenario_id}")
        scenario_ids.add(scenario_id)
        _positive_seconds(scenario.get("timeout_seconds"), f"{scenario_label}.timeout_seconds", default_timeout)
        _string_list(scenario.get("forbidden_markers"), f"{scenario_label}.forbidden_markers")

        processes = scenario.get("processes")
        if not isinstance(processes, list) or not processes:
            raise ManifestError(f"{scenario_label}.processes must be a non-empty array")
        process_ids: set[str] = set()
        for process_index, process in enumerate(processes):
            process_label = f"{scenario_label}.processes[{process_index}]"
            if not isinstance(process, dict):
                raise ManifestError(f"{process_label} must be an object")
            _unknown_fields(process, PROCESS_FIELDS, process_label)
            process_id = _required_string(process, "id", process_label)
            if process_id in process_ids:
                raise ManifestError(f"duplicate process id in {scenario_id}: {process_id}")
            process_ids.add(process_id)

            command = process.get("command")
            if not isinstance(command, list) or not command or any(not isinstance(arg, str) or not arg for arg in command):
                raise ManifestError(f"{process_label}.command must be a non-empty array of non-empty strings")
            working_directory = process.get("working_directory")
            if working_directory is not None and (not isinstance(working_directory, str) or not working_directory):
                raise ManifestError(f"{process_label}.working_directory must be a non-empty string")
            environment = process.get("environment", {})
            if not isinstance(environment, dict) or any(
                not isinstance(key, str) or not key or not isinstance(item, str)
                for key, item in environment.items()
            ):
                raise ManifestError(f"{process_label}.environment must map non-empty strings to strings")
            ready_marker = process.get("ready_marker")
            if ready_marker is not None and (not isinstance(ready_marker, str) or not ready_marker):
                raise ManifestError(f"{process_label}.ready_marker must be a non-empty string")
            if process.get("ready_timeout_seconds") is not None:
                _positive_seconds(process["ready_timeout_seconds"], f"{process_label}.ready_timeout_seconds")
            _string_list(process.get("required_markers"), f"{process_label}.required_markers")
            _string_list(process.get("forbidden_markers"), f"{process_label}.forbidden_markers")
            expected_exit = process.get("expected_exit_code", 0)
            if isinstance(expected_exit, bool) or not isinstance(expected_exit, int):
                raise ManifestError(f"{process_label}.expected_exit_code must be an integer")

    return manifest


def parse_values(entries: Sequence[str]) -> dict[str, str]:
    values: dict[str, str] = {}
    for entry in entries:
        if "=" not in entry:
            raise ManifestError(f"--value must use KEY=VALUE syntax: {entry}")
        key, value = entry.split("=", 1)
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", key) or not value:
            raise ManifestError(f"invalid --value entry: {entry}")
        if key in values:
            raise ManifestError(f"duplicate --value key: {key}")
        values[key] = value
    return values


def resolve_text(value: str, values: Mapping[str, str], label: str) -> str:
    missing = sorted({match.group(1) for match in PLACEHOLDER_RE.finditer(value) if match.group(1) not in values})
    if missing:
        raise ManifestError(f"{label} uses unresolved placeholder(s): {', '.join(missing)}")
    return PLACEHOLDER_RE.sub(lambda match: values[match.group(1)], value)


def write_process_output(label: str, output: str) -> None:
    encoding = sys.stdout.encoding or "utf-8"
    safe_output = output.encode(encoding, errors="replace").decode(encoding)
    for line in safe_output.splitlines():
        print(f"[{label}] {line}", flush=True)


class CapturedProcess:
    def __init__(
        self,
        label: str,
        command: Sequence[str],
        working_directory: str | None,
        environment: Mapping[str, str],
    ) -> None:
        self.label = label
        self.command = list(command)
        self.lines: list[str] = []
        self.lock = threading.Lock()
        process_environment = dict(os.environ)
        process_environment.update(environment)
        self.process = subprocess.Popen(
            self.command,
            cwd=working_directory,
            env=process_environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        self.thread = threading.Thread(target=self._capture, daemon=True)
        self.thread.start()

    def _capture(self) -> None:
        assert self.process.stdout is not None
        for line in self.process.stdout:
            with self.lock:
                self.lines.append(line)
            write_process_output(self.label, line)

    def output(self) -> str:
        with self.lock:
            return "".join(self.lines)

    def wait_for_marker(self, marker: str, deadline: float) -> bool:
        while time.monotonic() < deadline:
            if marker in self.output():
                return True
            if self.process.poll() is not None:
                return marker in self.output()
            time.sleep(0.05)
        return marker in self.output()

    def stop(self) -> None:
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
        self.thread.join(timeout=2)


def run_scenario(
    suite_name: str,
    scenario: Mapping[str, Any],
    default_timeout: float,
    suite_forbidden: Sequence[str],
    values: Mapping[str, str],
) -> dict[str, Any]:
    scenario_id = str(scenario["id"])
    timeout = _positive_seconds(scenario.get("timeout_seconds"), f"scenario {scenario_id} timeout", default_timeout)
    deadline = time.monotonic() + timeout
    started_at = time.monotonic()
    scenario_forbidden = [*suite_forbidden, *_string_list(scenario.get("forbidden_markers"), "forbidden markers")]
    launched: list[tuple[Mapping[str, Any], CapturedProcess]] = []
    reasons: list[str] = []
    scenario_timed_out = False

    print(f"[gameplay-test] scenario {scenario_id}: start", flush=True)
    try:
        for process_spec in scenario["processes"]:
            process_id = str(process_spec["id"])
            command = [
                resolve_text(arg, values, f"{scenario_id}.{process_id}.command")
                for arg in process_spec["command"]
            ]
            working_directory = process_spec.get("working_directory")
            resolved_working_directory = (
                resolve_text(working_directory, values, f"{scenario_id}.{process_id}.working_directory")
                if working_directory is not None
                else None
            )
            environment = {
                key: resolve_text(value, values, f"{scenario_id}.{process_id}.environment.{key}")
                for key, value in process_spec.get("environment", {}).items()
            }
            try:
                process = CapturedProcess(
                    f"{suite_name}:{scenario_id}:{process_id}",
                    command,
                    resolved_working_directory,
                    environment,
                )
            except OSError as error:
                reasons.append(f"process {process_id} failed to start: {error}")
                break
            launched.append((process_spec, process))

            ready_marker = process_spec.get("ready_marker")
            if ready_marker is not None:
                ready_timeout = _positive_seconds(
                    process_spec.get("ready_timeout_seconds"),
                    f"{scenario_id}.{process_id}.ready_timeout_seconds",
                    timeout,
                )
                ready_deadline = min(deadline, time.monotonic() + ready_timeout)
                if not process.wait_for_marker(ready_marker, ready_deadline):
                    reasons.append(f"process {process_id} did not emit ready marker {ready_marker!r}")
                    if time.monotonic() >= deadline:
                        scenario_timed_out = True
                    break

        if not reasons:
            for process_spec, process in launched:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    scenario_timed_out = True
                    reasons.append(f"scenario exceeded {timeout:g} second timeout")
                    break
                try:
                    process.process.wait(timeout=remaining)
                except subprocess.TimeoutExpired:
                    scenario_timed_out = True
                    reasons.append(f"scenario exceeded {timeout:g} second timeout")
                    break
    finally:
        for _, process in reversed(launched):
            process.stop()

    process_reports: list[dict[str, Any]] = []
    for process_spec, process in launched:
        process_id = str(process_spec["id"])
        output = process.output()
        required = _string_list(process_spec.get("required_markers"), "required markers")
        forbidden = [
            *scenario_forbidden,
            *_string_list(process_spec.get("forbidden_markers"), "forbidden markers"),
        ]
        missing = [marker for marker in required if marker not in output]
        forbidden_found = [marker for marker in forbidden if marker in output]
        expected_exit = int(process_spec.get("expected_exit_code", 0))
        exit_code = process.process.returncode
        if exit_code != expected_exit:
            reasons.append(f"process {process_id} exited {exit_code}, expected {expected_exit}")
        if missing:
            reasons.append(f"process {process_id} missed marker(s): {', '.join(missing)}")
        if forbidden_found:
            reasons.append(f"process {process_id} emitted forbidden marker(s): {', '.join(forbidden_found)}")
        process_reports.append(
            {
                "id": process_id,
                "exit_code": exit_code,
                "expected_exit_code": expected_exit,
                "missing_markers": missing,
                "forbidden_markers": forbidden_found,
            }
        )

    status = "passed" if not reasons else "failed"
    duration_ms = int((time.monotonic() - started_at) * 1000)
    print(f"[gameplay-test] scenario {scenario_id}: {status} ({duration_ms} ms)", flush=True)
    for reason in reasons:
        print(f"[gameplay-test] scenario {scenario_id}: {reason}", file=sys.stderr, flush=True)
    return {
        "id": scenario_id,
        "status": status,
        "duration_ms": duration_ms,
        "timed_out": scenario_timed_out,
        "reasons": reasons,
        "processes": process_reports,
    }


def run_manifest(manifest: Mapping[str, Any], values: Mapping[str, str]) -> dict[str, Any]:
    suite_name = str(manifest["name"])
    default_timeout = float(manifest["default_timeout_seconds"])
    suite_forbidden = _string_list(manifest.get("forbidden_markers"), "manifest.forbidden_markers")
    started_at = time.monotonic()
    scenarios = [
        run_scenario(suite_name, scenario, default_timeout, suite_forbidden, values)
        for scenario in manifest["scenarios"]
    ]
    passed_count = sum(scenario["status"] == "passed" for scenario in scenarios)
    report = {
        "schema_version": REPORT_SCHEMA_VERSION,
        "generated_by": "BuildTools/gameplay_test_runner.py",
        "suite": suite_name,
        "status": "passed" if passed_count == len(scenarios) else "failed",
        "duration_ms": int((time.monotonic() - started_at) * 1000),
        "scenario_count": len(scenarios),
        "passed_count": passed_count,
        "failed_count": len(scenarios) - passed_count,
        "scenarios": scenarios,
    }
    print(
        f"[gameplay-test] summary: suite={suite_name} status={report['status']} "
        f"scenarios={len(scenarios)} passed={passed_count} failed={len(scenarios) - passed_count}",
        flush=True,
    )
    return report


def write_report(path: Path, report: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8", newline="\n")


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="gameplay_test_runner.py",
        description="Run project-neutral multi-process gameplay smoke scenarios from a checked JSON manifest.",
    )
    parser.add_argument("--manifest", required=True, help="scenario manifest path")
    parser.add_argument("--value", action="append", default=[], metavar="KEY=VALUE", help="placeholder value; repeat as needed")
    parser.add_argument("--report", help="optional JSON result path")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    try:
        manifest = load_manifest(Path(args.manifest).resolve())
        values = parse_values(args.value)
        report = run_manifest(manifest, values)
    except ManifestError as error:
        print(f"[gameplay-test] invalid configuration: {error}", file=sys.stderr)
        return 2

    if args.report:
        write_report(Path(args.report).resolve(), report)
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
