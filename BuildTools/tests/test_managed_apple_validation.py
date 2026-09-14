from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import sys

import pytest


BUILDTOOLS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(BUILDTOOLS_DIR))

import buildtools


@pytest.mark.parametrize(
    ("name", "platform", "ios_target"),
    [
        ("managed-mac-client", "mac", None),
        ("managed-ios-simulator-client", "ios", "SIMULATOR64"),
        ("managed-ios-device-client", "ios", "OS64"),
    ],
)
def test_managed_validation_uses_explicit_backend_and_target(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch, name: str, platform: str, ios_target: str | None,
) -> None:
    monkeypatch.setenv("FO_ENGINE_ROOT", str(BUILDTOOLS_DIR.parent))
    monkeypatch.setenv("FO_WORKSPACE", str(tmp_path / "workspace"))
    monkeypatch.setattr(buildtools, "resolve_apple_cmake", lambda: "cmake")
    configured = []
    built = []
    monkeypatch.setattr(buildtools, "run_platform_configure_step", lambda *args: configured.append(args))
    monkeypatch.setattr(buildtools, "run_platform_build_step", lambda *args: built.append(args))
    env = buildtools.resolve_env()

    buildtools.run_validation(name, env)

    assert len(configured) == len(built) == 1
    command = configured[0][1]
    assert configured[0][0] == built[0][0] == platform
    assert built[0][2] == "Release"
    assert "-DFO_MANAGED_SCRIPTING=ON" in command
    assert "-DFO_ANGELSCRIPT_SCRIPTING=OFF" in command
    assert "-DFO_BUILD_CLIENT=1" in command
    assert "Xcode" in command
    assert configured[0][2].name == f"validate-{name}"
    assert Path(command[-1]).joinpath("FOnlineTest.fomain").is_file()
    targets = [arg.split("=", 1)[1] for arg in command if arg.startswith("-DPLATFORM=")]
    assert (targets[-1] if targets else None) == ios_target
    if ios_target == "OS64":
        assert "-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO" in command
    else:
        assert not any(arg.startswith("-DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=") for arg in command)


@pytest.mark.skipif(shutil.which("cmake") is None, reason="CMake is required")
@pytest.mark.parametrize("managed", [False, True])
def test_actual_scaffold_defaults_remain_overridable_without_changing_other_validators(tmp_path: Path, managed: bool) -> None:
    source = (BUILDTOOLS_DIR / "validation-project/CMakeLists.txt").read_text()
    source = source[:source.index("StartProjectGeneration()")]
    source = source.replace("project(FOnlineTest)", "project(FOnlineTest NONE)")
    source = source.replace("include(Engine/BuildTools/Init.cmake)", f'include("{(BUILDTOOLS_DIR / "Init.cmake").as_posix()}")')
    source += 'file(WRITE "${CMAKE_BINARY_DIR}/backends.txt" "${FO_MANAGED_SCRIPTING}|${FO_ANGELSCRIPT_SCRIPTING}")\n'
    (tmp_path / "CMakeLists.txt").write_text(source)
    build = tmp_path / "build"
    arguments = list(buildtools.MANAGED_VALIDATION_CMAKE_ARGS) if managed else []

    result = subprocess.run(
        ["cmake", "-S", str(tmp_path), "-B", str(build), *arguments],
        capture_output=True, text=True, timeout=30,
    )

    assert result.returncode == 0, result.stdout + result.stderr
    assert (build / "backends.txt").read_text() == ("ON|OFF" if managed else "OFF|ON")
    for name in ("mac-client", "ios-client", "unit-tests"):
        assert "cmake_args" not in buildtools.VALIDATION_TARGETS[name]
