from __future__ import annotations

from pathlib import Path
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))

import buildtools


@pytest.mark.parametrize(
    ("os_name", "system_platform", "expected"),
    [
        ("nt", "win32", "win64"),
        ("posix", "linux", "linux"),
        ("posix", "darwin", "mac"),
    ],
)
def test_native_validation_platform_matches_the_host(
    monkeypatch: pytest.MonkeyPatch, os_name: str, system_platform: str, expected: str,
) -> None:
    monkeypatch.setattr(buildtools.os, "name", os_name)
    monkeypatch.setattr(buildtools.sys, "platform", system_platform)

    assert buildtools.resolve_validation_platform("native") == expected


def test_explicit_validation_platform_is_unchanged() -> None:
    assert buildtools.resolve_validation_platform("linux") == "linux"


def test_unit_tests_use_the_native_platform() -> None:
    assert buildtools.VALIDATION_TARGETS["unit-tests"]["platform"] == "native"


def test_unit_validation_passes_the_resolved_platform_to_cmake(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch,
) -> None:
    configured: list[tuple[object, ...]] = []
    run_targets: list[tuple[object, ...]] = []
    monkeypatch.setattr(buildtools, "resolve_validation_platform", lambda _platform: "win64")
    monkeypatch.setattr(buildtools, "prepare_validation_project", lambda _env: tmp_path / "source")
    monkeypatch.setattr(buildtools, "run_platform_configure_build", lambda *args, **_kwargs: configured.append(args))
    monkeypatch.setattr(buildtools, "run_cmake_target", lambda *args, **_kwargs: run_targets.append(args))
    env = {
        "FO_WORKSPACE": str(tmp_path / "workspace"),
        "FO_ENGINE_ROOT": str(tmp_path / "engine"),
    }

    buildtools.run_validation("unit-tests", env)

    assert configured[0][:2] == ("win64", "unit-tests")
    assert run_targets[0][1:] == ("Release", "RunUnitTests")


def test_windows_native_configure_rejects_cached_unix_generator(tmp_path: Path) -> None:
    (tmp_path / "CMakeCache.txt").write_text("CMAKE_GENERATOR:INTERNAL=Unix Makefiles\n")

    assert buildtools._cached_generator_mismatch(tmp_path, ["cmake", "-A", "x64", "source"])


def test_windows_native_configure_accepts_cached_visual_studio_generator(tmp_path: Path) -> None:
    (tmp_path / "CMakeCache.txt").write_text("CMAKE_GENERATOR:INTERNAL=Visual Studio 18 2026\n")

    assert not buildtools._cached_generator_mismatch(tmp_path, ["cmake", "-A", "x64", "source"])


def test_unknown_native_host_is_rejected(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.setattr(buildtools.os, "name", "posix")
    monkeypatch.setattr(buildtools.sys, "platform", "plan9")

    with pytest.raises(SystemExit, match="Unsupported native validation host: plan9"):
        buildtools.resolve_validation_platform("native")
