"""Host workspace vocabulary, wrapper forwarding and the BuildTools Python floor."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
ENGINE_ROOT = BUILDTOOLS_DIR.parent
BUILDTOOLS_PY = BUILDTOOLS_DIR / "buildtools.py"

sys.path.insert(0, str(BUILDTOOLS_DIR))
import buildtools  # noqa: E402


HOSTS = ("linux", "windows", "macos")
SHELL_WRAPPERS = ("build", "validate", "toolset", "format-source", "setup-mono", "setup-env", "prepare-workspace", "prepare-mac-workspace")
CMD_WRAPPERS = ("build", "validate", "toolset", "format-source", "setup-mono", "setup-env")


def _prepare_host_choices(dest: str) -> list[str]:
    parser = buildtools.create_parser()
    subparsers = next(action for action in parser._actions if action.dest == "command")
    prepare_host = subparsers.choices["prepare-host-workspace"]
    return list(next(action for action in prepare_host._actions if action.dest == dest).choices)


def _run_buildtools(*args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run([sys.executable, str(BUILDTOOLS_PY), *args], capture_output=True, text=True, check=False)


def test_every_host_feature_prepares_something() -> None:
    for host_name in HOSTS:
        for feature in buildtools.list_host_features(host_name):
            package_groups = buildtools.HOST_FEATURE_PACKAGE_GROUPS[host_name].get(feature, [])
            parts = buildtools.HOST_FEATURE_WORKSPACE_PARTS[host_name].get(feature, [])
            assert package_groups or parts, (host_name, feature)


def test_host_vocabulary_matches_the_tables() -> None:
    assert buildtools.list_host_features("linux") == [
        "common-packages",
        "linux-packages",
        "web-packages",
        "android-packages",
        "windows-cross-packages",
        "msi-packages",
        "all-packages",
        "toolset",
        "web",
        "android-arm32",
        "android-arm64",
        "android-x86",
        "dotnet",
        "windows-cross",
        "msan-libcxx",
        "all",
    ]
    assert buildtools.list_host_features("windows") == ["toolset", "web", "wix", "all"]
    assert buildtools.list_host_features("macos") == []


def test_parser_choices_are_derived_from_the_host_tables() -> None:
    assert _prepare_host_choices("host") == list(HOSTS)
    assert _prepare_host_choices("features") == buildtools.list_all_host_features()
    assert "linux" not in buildtools.list_all_host_features()
    assert "packages" not in buildtools.list_all_host_features()


def test_host_defaults_are_accepted_by_their_host() -> None:
    for host_name in HOSTS:
        assert buildtools.describe_unsupported_host_features(host_name, buildtools.HOST_DEFAULT_FEATURES[host_name]) == ""


@pytest.mark.parametrize(
    ("host_name", "features", "unsupported"),
    [
        ("windows", ["toolset", "dotnet"], "dotnet"),
        ("windows", ["android-arm64", "msi-packages"], "android-arm64, msi-packages"),
        ("linux", ["linux-packages", "wix"], "wix"),
        ("macos", ["web"], "web"),
    ],
)
def test_names_the_host_does_not_support_are_listed(host_name: str, features: list[str], unsupported: str) -> None:
    message = buildtools.describe_unsupported_host_features(host_name, features)

    assert message.startswith(f"prepare-host-workspace {host_name} does not support {unsupported}: ")
    assert buildtools.describe_host_features(host_name) in message


def test_macos_states_that_it_only_runs_the_host_check() -> None:
    assert buildtools.describe_host_features("macos") == (
        "macos accepts no features; prepare-host-workspace macos only runs the host check"
    )


def test_unsupported_host_feature_is_an_argument_error() -> None:
    result = _run_buildtools("prepare-host-workspace", "windows", "dotnet", "--check")

    assert result.returncode == 2
    assert "prepare-host-workspace windows does not support dotnet: windows accepts toolset, web, wix, all" in result.stderr
    assert result.stdout == ""


def test_unknown_feature_is_an_argument_error() -> None:
    result = _run_buildtools("prepare-host-workspace", "linux", "linux-packages", "linux")

    assert result.returncode == 2
    assert "invalid choice: 'linux'" in result.stderr


def test_interpreter_below_the_floor_is_refused_with_a_message() -> None:
    script = (
        "import runpy, sys\n"
        "sys.version_info = (3, 11, 9, 'final', 0)\n"
        "sys.argv = ['buildtools.py', 'host-check', 'linux']\n"
        f"runpy.run_path({str(BUILDTOOLS_PY)!r}, run_name='__main__')\n"
    )
    result = subprocess.run([sys.executable, "-c", script], capture_output=True, text=True, check=False)

    assert result.returncode == 1
    assert "BuildTools requires Python 3.12 or newer" in result.stderr
    assert "is Python 3.11.9" in result.stderr


def test_wrappers_probe_the_buildtools_python_floor() -> None:
    floor = f"sys.version_info < ({buildtools.MIN_PYTHON_VERSION[0]}, {buildtools.MIN_PYTHON_VERSION[1]})"
    floor_text = f"Python {buildtools.MIN_PYTHON_VERSION[0]}.{buildtools.MIN_PYTHON_VERSION[1]} or newer not found"

    for helper in ("find-python.sh", "find-python.cmd", "prepare-win-workspace.ps1"):
        text = (BUILDTOOLS_DIR / helper).read_text(encoding="utf-8")
        assert floor in text, helper
        assert floor_text in text, helper

    for name in SHELL_WRAPPERS:
        text = (BUILDTOOLS_DIR / f"{name}.sh").read_text(encoding="utf-8")
        assert 'source "$CUR_DIR/find-python.sh"' in text, name
        assert "command -v python3" not in text, name
    for name in CMD_WRAPPERS:
        text = (BUILDTOOLS_DIR / f"{name}.cmd").read_text(encoding="utf-8")
        assert 'call "%~dp0find-python.cmd" || exit /b 1' in text, name
        assert "where py" not in text, name


def test_wrappers_keep_no_feature_list_of_their_own() -> None:
    for wrapper in ("prepare-workspace.sh", "prepare-mac-workspace.sh", "prepare-win-workspace.ps1"):
        text = (BUILDTOOLS_DIR / wrapper).read_text(encoding="utf-8")
        assert not re.search(r"known_?features", text, flags=re.IGNORECASE), wrapper
        for feature in ("android-arm64", "msi-packages", "toolset", "wix"):
            assert feature not in text, (wrapper, feature)


def test_format_source_patterns_cover_every_engine_source_suffix() -> None:
    # A .fos file under Source would need its pattern here; .fos repair otherwise serves project format_files calls
    assert buildtools.FORMAT_PATTERNS == ["**/*.cpp", "**/*.h", "**/*.cs"]
    assert not list((ENGINE_ROOT / "Source").rglob("*.fos"))


def _write_stub(path: Path, body: str) -> None:
    path.write_text("#!/bin/bash\n" + body, encoding="utf-8", newline="\n")
    path.chmod(0o755)


@pytest.mark.skipif(os.name == "nt", reason="POSIX wrappers run under a POSIX bash")
def test_find_python_skips_an_interpreter_below_the_floor(tmp_path: Path) -> None:
    stubs = tmp_path / "stubs"
    tools = tmp_path / "tools"
    stubs.mkdir()
    tools.mkdir()
    for tool in ("grep", "sort"):
        tool_path = shutil.which(tool)
        assert tool_path is not None, tool
        (tools / tool).symlink_to(tool_path)

    _write_stub(stubs / "python3", "exit 1\n")
    _write_stub(stubs / "python3.10", "exit 1\n")
    _write_stub(stubs / "python3.13", f'exec "{sys.executable}" "$@"\n')
    script = f'source "{BUILDTOOLS_DIR / "find-python.sh"}"; echo "$PYTHON_BIN"'
    environment = {"PATH": f"{stubs}:{tools}"}

    result = subprocess.run(["bash", "-e", "-c", script], env=environment, capture_output=True, text=True, check=False)

    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == str(stubs / "python3.13")

    (stubs / "python3.13").unlink()
    result = subprocess.run(["bash", "-e", "-c", script], env=environment, capture_output=True, text=True, check=False)

    assert result.returncode == 1
    assert "Python 3.12 or newer not found" in result.stderr


@pytest.mark.skipif(os.name == "nt", reason="POSIX wrappers run under a POSIX bash")
@pytest.mark.parametrize(
    ("wrapper", "arguments", "expected"),
    [
        ("prepare-workspace.sh", ["check", "msi-packages", "misspelled"], "prepare-host-workspace linux msi-packages misspelled --check"),
        ("prepare-mac-workspace.sh", ["packages", "check"], "prepare-host-workspace macos packages --check"),
    ],
)
def test_shell_wrappers_forward_every_feature_name(tmp_path: Path, wrapper: str, arguments: list[str], expected: str) -> None:
    log_path = tmp_path / "args.txt"
    _write_stub(
        tmp_path / "python3",
        f'if [[ "$1" == "-c" ]]; then exit 0; fi\nshift\necho "$*" > "{log_path}"\nexit 0\n',
    )

    result = subprocess.run(
        ["bash", str(BUILDTOOLS_DIR / wrapper), *arguments],
        env={"PATH": f"{tmp_path}:/usr/bin:/bin"},
        capture_output=True,
        text=True,
        check=False,
    )

    assert result.returncode == 0, result.stderr
    assert log_path.read_text(encoding="utf-8").strip() == expected
