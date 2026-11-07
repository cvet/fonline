"""Check the copyable script test harness example: backend parity, the settings snippet, the runner manifest,
and the managed harness compiled and executed in its stand-in host."""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
from contextlib import redirect_stderr, redirect_stdout
from io import StringIO
from pathlib import Path

import pytest


ENGINE_ROOT = Path(__file__).resolve().parents[2]
EXAMPLE = ENGINE_ROOT / "Examples/GameplayTestHarness/ScriptTests"
AS_HARNESS = EXAMPLE / "AngelScript/Testing.fos"
AS_SUITE = EXAMPLE / "AngelScript/Test_HarnessExample.fos"
CS_HARNESS = EXAMPLE / "Managed/Testing.cs"
CS_SUITE = EXAMPLE / "Managed/Test_HarnessExample.cs"
HOST_PROJECT = EXAMPLE / "Managed/Verification/ScriptTestHost.csproj"
SETTINGS_SNIPPET = EXAMPLE / "script-tests.fomain"
MANIFEST = EXAMPLE / "script-tests.json"
GUIDE = ENGINE_ROOT / "Docs/en/how-to/project/script-test-harness.md"
sys.path.insert(0, str(ENGINE_ROOT / "BuildTools"))

import gameplay_test_runner  # noqa: E402


SETTING_RE = re.compile(r"^\s*///@ Setting (\w+) (\w+) (Testing\.\w+)\s*$", re.MULTILINE)
EVENT_RE = re.compile(r"^\s*///@ Event .+$", re.MULTILINE)
MARKER_FIELD_RE = re.compile(r'"\s?(\w+)="')
PASSING_SUMMARY = "[Testing] COMPLETE status=passed registered=2 selected=2 passed=2 failed=0 skipped=0 not_run=0"


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _root_section_keys(config_text: str) -> set[str]:
    keys: set[str] = set()
    for line in config_text.splitlines():
        stripped = line.strip()
        if stripped.startswith("["):
            break
        if stripped and not stripped.startswith("#"):
            keys.add(stripped.split("=", 1)[0].strip())
    return keys


def test_backends_declare_the_same_settings_and_event() -> None:
    angelscript = _read(AS_HARNESS)
    managed = _read(CS_HARNESS)

    as_settings = SETTING_RE.findall(angelscript)
    cs_settings = SETTING_RE.findall(managed)
    assert as_settings == cs_settings
    assert [name for _, _, name in as_settings] == [
        "Testing.Enabled",
        "Testing.Filter",
        "Testing.StopOnFirstFailure",
        "Testing.ExitOnFinish",
        "Testing.TestTimeout",
    ]
    assert {side for side, _, _ in as_settings} == {"Server"}
    assert [line.strip() for line in EVENT_RE.findall(angelscript)] == ["///@ Event Server Game OnTestingInit()"]
    assert [line.strip() for line in EVENT_RE.findall(managed)] == ["///@ Event Server Game OnTestingInit()"]


def test_backends_emit_the_same_marker_vocabulary() -> None:
    angelscript = _read(AS_HARNESS)
    managed = _read(CS_HARNESS)

    for text in (angelscript, managed):
        assert 'MarkerPrefix = "[Testing] "' in text
        for word in ('"START "', '"PASS "', '"FAIL "', '"ERROR "', '"COMPLETE "', '"timeout"'):
            assert word in text
    assert MARKER_FIELD_RE.findall(angelscript) == MARKER_FIELD_RE.findall(managed)
    assert MARKER_FIELD_RE.findall(angelscript) == [
        "registered",
        "selected",
        "status",
        "registered",
        "selected",
        "passed",
        "failed",
        "skipped",
        "not_run",
    ]


def test_settings_snippet_configures_every_declared_setting() -> None:
    snippet = _read(SETTINGS_SNIPPET)
    declared = {name for _, _, name in SETTING_RE.findall(_read(AS_HARNESS))}

    assert declared <= _root_section_keys(snippet)
    assert "Testing.Enabled = False" in snippet
    assert "AngelScript.MutableGlobalsAllowedNamespaces = Testing" in snippet
    assert re.search(r"^\[SubConfig\]\s*\nName = ScriptTests\s*\nTesting\.Enabled = True$", snippet, re.MULTILINE)


def test_angelscript_files_keep_engine_module_layout() -> None:
    for path in (AS_HARNESS, AS_SUITE):
        text = _read(path)
        assert re.search(rf"^namespace {re.escape(path.stem)}$", text, re.MULTILINE), path
        assert text.count("{") == text.count("}"), path
        assert len(re.findall(r"^#if SERVER$", text, re.MULTILINE)) == len(re.findall(r"^#endif$", text, re.MULTILINE)) == 1
        assert "@" not in text.replace("///@", ""), path


def test_example_files_end_with_one_newline_and_no_bom() -> None:
    for path in sorted(EXAMPLE.rglob("*")):
        if path.is_file() and path.suffix in {".fos", ".cs", ".csproj", ".fomain", ".json"} and "obj" not in path.parts:
            raw = path.read_bytes().replace(b"\r\n", b"\n")
            assert not raw.startswith(b"\xef\xbb\xbf"), path
            assert raw.endswith(b"\n") and not raw.endswith(b"\n\n"), path


def test_manifest_requires_the_passing_summary_and_forbids_failures() -> None:
    manifest = gameplay_test_runner.load_manifest(MANIFEST)
    process = manifest["scenarios"][0]["processes"][0]

    assert process["command"] == ["{server}", "-ApplyConfig", "{config}", "-ApplySubConfig", "ScriptTests"]
    assert "[Testing] COMPLETE status=passed" in process["required_markers"]
    assert PASSING_SUMMARY.startswith("[Testing] COMPLETE status=passed")
    for marker in ("[Testing] FAIL", "[Testing] ERROR", "[Testing] COMPLETE status=failed"):
        assert marker in manifest["forbidden_markers"]


def _stripped_lines(text: str) -> list[str]:
    return [line.strip() for line in text.splitlines()]


def _contains_lines(haystack: list[str], needle: list[str]) -> bool:
    return any(haystack[index:index + len(needle)] == needle for index in range(len(haystack) - len(needle) + 1))


def test_guide_states_the_marker_contract_and_quotes_the_example_verbatim() -> None:
    guide = _read(GUIDE)

    assert "[Testing] COMPLETE status=passed" in guide
    for source in (AS_HARNESS, AS_SUITE, CS_HARNESS, CS_SUITE, SETTINGS_SNIPPET, MANIFEST):
        assert f"`{source.relative_to(ENGINE_ROOT).as_posix()}`" in guide

    fences = re.findall(r"^```(angelscript|csharp|json)\n(.*?)^```$", guide.replace("\r\n", "\n"), re.MULTILINE | re.DOTALL)
    sources = {
        "angelscript": [_stripped_lines(_read(path)) for path in (AS_HARNESS, AS_SUITE)],
        "csharp": [_stripped_lines(_read(path)) for path in (CS_HARNESS, CS_SUITE)],
    }
    assert [language for language, _ in fences].count("angelscript") == 2
    assert [language for language, _ in fences].count("csharp") == 2
    for language, body in fences:
        if language == "json":
            assert json.loads(body) == json.loads(_read(MANIFEST))
            continue
        assert any(_contains_lines(lines, _stripped_lines(body.rstrip("\n"))) for lines in sources[language]), body


@pytest.fixture(scope="module")
def host(tmp_path_factory: pytest.TempPathFactory) -> Path:
    dotnet = shutil.which("dotnet")
    if dotnet is None:
        pytest.skip("dotnet SDK is required to compile the managed harness stand-in host")

    artifacts = tmp_path_factory.mktemp("script-test-host").resolve()
    command = [dotnet, "build", str(HOST_PROJECT), "-c", "Release", "--nologo", "-v:minimal", "--artifacts-path", str(artifacts)]
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=300)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "warning" not in result.stdout.lower().replace("0 warning(s)", ""), result.stdout

    executables = sorted(artifacts.glob("bin/ScriptTestHost/*/ScriptTestHost.exe")) or sorted(artifacts.glob("bin/ScriptTestHost/*/ScriptTestHost"))
    assert executables, "the build did not produce the ScriptTestHost apphost"
    return executables[0]


def _run_host(host: Path, *settings: str, sub_config: bool = True) -> tuple[int, list[str]]:
    command = [str(host), "-ApplyConfig", str(SETTINGS_SNIPPET)]
    if sub_config:
        command += ["-ApplySubConfig", "ScriptTests"]
    command += list(settings)
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
    return result.returncode, result.stdout.splitlines()


def test_passing_run_logs_every_test_and_quits_with_success(host: Path) -> None:
    exit_code, lines = _run_host(host)

    assert exit_code == 0
    assert lines == [
        "[Testing] START registered=2 selected=2 filter=''",
        "[Testing] PASS harness_example.split_drops_empty_entries",
        "[Testing] PASS harness_example.awaited_time_step_completes_the_test",
        PASSING_SUMMARY,
    ]


def test_failures_exceptions_and_timeouts_fail_the_run(host: Path) -> None:
    exit_code, lines = _run_host(host, "-HarnessHost.Mode", "faults")

    assert exit_code == 1
    assert "[Testing] FAIL selfcheck.expectation_fails: Deliberate mismatch (actual 1, expected 2)" in lines
    assert "[Testing] FAIL selfcheck.exception_fails: exception: InvalidOperationException: Deliberate failure" in lines
    assert "[Testing] FAIL selfcheck.suspended_step_times_out: timeout" in lines
    assert "[Testing] FAIL selfcheck.unfinished_task_times_out: timeout" in lines
    assert "[Testing] PASS selfcheck.first_verdict_wins" in lines
    assert lines[-1] == "[Testing] COMPLETE status=failed registered=7 selected=7 passed=3 failed=4 skipped=0 not_run=0"


def test_stop_on_first_failure_counts_tests_not_run(host: Path) -> None:
    exit_code, lines = _run_host(host, "-HarnessHost.Mode", "faults", "-Testing.StopOnFirstFailure", "True")

    assert exit_code == 1
    assert sum(line.startswith("[Testing] FAIL ") for line in lines) == 1
    assert lines[-1] == "[Testing] COMPLETE status=failed registered=7 selected=7 passed=2 failed=1 skipped=0 not_run=4"


def test_filter_selects_by_case_insensitive_tokens(host: Path) -> None:
    exit_code, lines = _run_host(host, "-Testing.Filter", "SPLIT")
    assert exit_code == 0
    assert lines[-1] == "[Testing] COMPLETE status=passed registered=2 selected=1 passed=1 failed=0 skipped=1 not_run=0"

    exit_code, lines = _run_host(host, "-Testing.Filter", "split, Awaited_Time")
    assert exit_code == 0
    assert lines[-1] == PASSING_SUMMARY


def test_empty_selection_and_configuration_errors_fail(host: Path) -> None:
    cases = (
        (("-Testing.Filter", "no_such_test"), "[Testing] ERROR no registered test matches the filter", "selected=0"),
        (("-Testing.TestTimeout", "0"), "[Testing] ERROR Testing.TestTimeout must be a positive number of milliseconds", "not_run=2"),
        (("-HarnessHost.Mode", "duplicate"), "[Testing] ERROR duplicate test 'harness_example.split_drops_empty_entries'", "not_run=2"),
        (("-HarnessHost.Mode", "invalid"), "[Testing] ERROR invalid registration 'self.check.dotted_suite_is_rejected'", "not_run=2"),
    )
    for settings, error, count in cases:
        exit_code, lines = _run_host(host, *settings)
        assert exit_code == 1, settings
        assert error in lines, lines
        assert lines[-1].startswith("[Testing] COMPLETE status=failed ") and count in lines[-1], lines


def test_disabled_harness_and_missing_exit_request_stay_silent(host: Path) -> None:
    exit_code, lines = _run_host(host, sub_config=False)
    assert exit_code == 3
    assert not any(line.startswith("[Testing]") for line in lines)

    exit_code, lines = _run_host(host, "-Testing.ExitOnFinish", "False")
    assert exit_code == 3
    assert PASSING_SUMMARY in lines


def _run_manifest(manifest_path: Path, host: Path, report_path: Path) -> int:
    argv = [
        "--manifest",
        str(manifest_path),
        "--value",
        f"server={host}",
        "--value",
        f"config={SETTINGS_SNIPPET}",
        "--report",
        str(report_path),
    ]
    with redirect_stdout(StringIO()), redirect_stderr(StringIO()):
        return gameplay_test_runner.main(argv)


def test_runner_accepts_the_passing_harness_and_rejects_a_failing_one(host: Path, tmp_path: Path) -> None:
    tmp_path = tmp_path.resolve()

    assert _run_manifest(MANIFEST, host, tmp_path / "pass.json") == 0
    assert json.loads((tmp_path / "pass.json").read_text(encoding="utf-8"))["status"] == "passed"

    failing = json.loads(_read(MANIFEST))
    failing["scenarios"][0]["processes"][0]["command"] += ["-HarnessHost.Mode", "faults"]
    failing_path = tmp_path / "failing.json"
    failing_path.write_text(json.dumps(failing), encoding="utf-8")

    assert _run_manifest(failing_path, host, tmp_path / "fail.json") == 1
    process = json.loads((tmp_path / "fail.json").read_text(encoding="utf-8"))["scenarios"][0]["processes"][0]
    assert process["exit_code"] == 1
    assert process["missing_markers"] == ["[Testing] COMPLETE status=passed"]
    assert process["forbidden_markers"] == ["[Testing] FAIL", "[Testing] COMPLETE status=failed"]
