from __future__ import annotations

import argparse
import json
import os
import queue
import subprocess
import sys
import threading
import time
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[2]
SAMPLE = Path(__file__).resolve().with_name("ai_control_sample.py")
sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))

from ai_control_client import AiControlClient, RemoteError  # noqa: E402


def _read_ready(process: subprocess.Popen[str], timeout: float) -> dict[str, object]:
    lines: queue.Queue[str] = queue.Queue()

    def read_line() -> None:
        assert process.stdout is not None
        lines.put(process.stdout.readline())

    threading.Thread(target=read_line, daemon=True).start()
    try:
        line = lines.get(timeout=timeout)
    except queue.Empty as exception:
        raise RuntimeError("sample did not publish its ready marker") from exception
    prefix = "AI_CONTROL_SAMPLE_READY "
    if not line.startswith(prefix):
        raise RuntimeError(f"unexpected sample startup output: {line.rstrip()}")
    value = json.loads(line[len(prefix) :])
    if not isinstance(value, dict):
        raise RuntimeError("sample ready marker is not a JSON object")
    return value


def run_smoke(timeout: float = 10.0) -> dict[str, object]:
    token = "sample-token"
    environment = dict(os.environ)
    environment["FONLINE_AI_TOKEN"] = token
    process = subprocess.Popen(
        [
            sys.executable,
            str(SAMPLE),
            "--host",
            "127.0.0.1",
            "--port",
            "0",
            "--token-env",
            "FONLINE_AI_TOKEN",
        ],
        cwd=ENGINE_ROOT,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
    )
    checks: list[str] = []
    try:
        ready = _read_ready(process, timeout)
        host = str(ready["host"])
        port = int(ready["port"])
        with AiControlClient(host, port, timeout=timeout) as client:
            if client.auth("wrong-token"):
                raise AssertionError("wrong token was authorized")
            checks.append("wrong-token-rejected")
            try:
                client.ping()
            except RemoteError as exception:
                if exception.code != -32001:
                    raise
            else:
                raise AssertionError("unauthorized ping was accepted")
            checks.append("unauthorized-method-rejected")
            if not client.auth(token):
                raise AssertionError("correct token was rejected")
            checks.append("authorized")
            if client.ping() != {"ok": True}:
                raise AssertionError("ping result differs")
            checks.append("ping")
            status = client.status()
            required_status = {
                "running",
                "host",
                "port",
                "queuedCommands",
                "maxQueuedCommands",
                "events",
                "maxEvents",
                "observationSeq",
                "lastError",
            }
            if not required_status.issubset(status):
                raise AssertionError("status is missing protocol fields")
            checks.append("status")
            initial = client.observe()
            if initial.get("observation", {}).get("agent") != {"x": 0, "y": 0}:
                raise AssertionError("initial observation differs")
            checks.append("observe")
            try:
                client.request("act", {})
            except RemoteError as exception:
                if exception.code != -32602:
                    raise
            else:
                raise AssertionError("act without type was accepted")
            checks.append("invalid-command-rejected")
            try:
                client.request("not-a-method", {})
            except RemoteError as exception:
                if exception.code != -32601:
                    raise
            else:
                raise AssertionError("unknown method was accepted")
            checks.append("unknown-method-rejected")
            accepted = client.act({"type": "move", "x": 7, "y": 9})
            command_seq = int(accepted["commandSeq"])
            if accepted.get("accepted") is not True:
                raise AssertionError("command was not accepted")
            checks.append("command-accepted")
            deadline = time.monotonic() + timeout
            completion: dict[str, object] | None = None
            latest_seq = 0
            while time.monotonic() < deadline:
                event_result = client.events(after_seq=latest_seq, limit=100)
                latest_seq = int(event_result["latestSeq"])
                for record in event_result["events"]:
                    event = record["event"]
                    if (
                        event.get("type") == "command_completed"
                        and event.get("commandSeq") == command_seq
                    ):
                        completion = event
                        break
                if completion is not None:
                    break
                time.sleep(0.02)
            if completion is None or completion.get("success") is not True:
                raise AssertionError("successful completion event was not observed")
            checks.append("completion-event")
            moved = client.observe()
            if moved.get("observation", {}).get("agent") != {"x": 7, "y": 9}:
                raise AssertionError("observation did not reflect the command")
            checks.append("observation-updated")
            empty = client.events(after_seq=latest_seq, limit=5000)
            if empty.get("events") != []:
                raise AssertionError("event cursor replayed old events")
            checks.append("event-cursor")
        return {
            "schema_version": 1,
            "status": "passed",
            "checks": checks,
            "check_count": len(checks),
        }
    finally:
        process.terminate()
        try:
            process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.communicate(timeout=5)


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run the AiControl reference client against the protocol sample.",
        allow_abbrev=False,
    )
    parser.add_argument("--timeout", type=float, default=10.0, help="Per-stage timeout in seconds.")
    parser.add_argument("--report", type=Path, help="Optional JSON report path.")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    try:
        report = run_smoke(args.timeout)
    except (AssertionError, OSError, RuntimeError, ValueError) as exception:
        print(f"AiControl protocol smoke failed: {exception}", file=sys.stderr)
        return 1
    content = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.report is not None:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(content, encoding="utf-8", newline="\n")
    print(content, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
